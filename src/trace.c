/*
  References:

  https://idea.popcount.org/2012-12-11-linux-process-states/
  http://strace.git.sourceforge.net/git/gitweb.cgi?p=strace/strace;a=blob;f=README-linux-ptrace;hb=HEAD
  http://hssl.cs.jhu.edu/~neal/woodchuck/src/commits/c8a6c6c6d28c87e2ef99e21cf76c4ea90a7e11ad/src/process-monitor-ptrace.c.raw.html
  http://www.linuxjournal.com/article/6210?page=0,1
*/

#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/signalfd.h>
#include <fcntl.h>
#include <sys/user.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>

#include "list.h"
#include "trace.h"
#include "types.h"
#include "fluxcapacitor.h"


extern struct options options;


#define HPIDS_SIZE 51


#if defined(__x86_64__) || defined(__i386__)

/* On x86-64, RAX is set to -ENOSYS on system call entry.  How
   do we distinguish this from a system call that returns
   ENOSYS?  (marek: we don't) */
# define SYSCALL_ENTRY (RET == -ENOSYS)
# define REGS_STRUCT struct user_regs_struct

# if defined(__x86_64__)

#  define SYSCALL (regs.orig_rax)
#  define ARG1 (regs.rdi)
#  define ARG2 (regs.rsi)
#  define ARG3 (regs.rdx)
#  define ARG4 (regs.r10)
#  define ARG5 (regs.r8)
#  define ARG6 (regs.r9)
#  define RET (regs.rax)

# elif defined(__i386__)

#  define SYSCALL (regs.orig_eax)
#  define ARG1 (regs.ebx)
#  define ARG2 (regs.ecx)
#  define ARG3 (regs.edx)
#  define ARG4 (regs.esi)
#  define ARG5 (regs.edi)
#  define ARG6 (regs.ebp)
#  define RET (regs.eax)

#endif

#elif defined(__arm__)

# define REGS_STRUCT struct pt_regs
/* ip is set to 0 on system call entry, 1 on exit.  */
# define SYSCALL_ENTRY (regs.ARM_ip == 0)

/* This layout assumes that there are no 64-bit parameters.  See
   http://lkml.org/lkml/2006/1/12/175 for the complications.  */
# define SYSCALL (regs.ARM_r7)
# define ARG1 (regs.ARM_ORIG_r0)
# define ARG2 (regs.ARM_r1)
# define ARG3 (regs.ARM_r2)
# define ARG4 (regs.ARM_r3)
# define ARG5 (regs.ARM_r4)
# define ARG6 (regs.ARM_r5)
# define RET (regs.ARM_r0)

#else

# error Not ported to your architecture.

#endif


struct trace {
	int sfd;
	int process_count;
	struct hlist_head hpids[HPIDS_SIZE];

	trace_callback callback;
	void *userdata;

	struct list_head list_of_waitpid_reports;
};

struct trace_process {
	struct trace *trace;
	struct hlist_node node;

	pid_t pid;
	int initialized;
	int within_syscall;
	int mem_fd;
	REGS_STRUCT regs;

	trace_callback callback;
	void *userdata;
};

struct waitpid_report {
	struct list_head in_list;

	int pid;
	int status;
};

static void trace_process_del(struct trace *trace, struct trace_process *process);

struct trace *trace_new(trace_callback callback, void *userdata) {
	struct trace *trace = calloc(1, sizeof(struct trace));

	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
		PFATAL("sigprocmask(SIG_BLOCK, [SIGCHLD])");

	trace->sfd = signalfd(-1, &mask, SFD_CLOEXEC);
	if (trace->sfd == -1)
		PFATAL("signalfd()");
	int i;
	for (i=0; i < HPIDS_SIZE; i++)
		INIT_HLIST_HEAD(&trace->hpids[i]);

	trace->callback = callback;
	trace->userdata = userdata;

	INIT_LIST_HEAD(&trace->list_of_waitpid_reports);
	return trace;
}

void trace_free(struct trace *trace) {
	close(trace->sfd);
	trace->sfd = -1;

	int i;
	for (i=0; i < HPIDS_SIZE; i++) {
		struct hlist_node *pos;
		hlist_for_each(pos, &trace->hpids[i]) {
			struct trace_process *process =
				hlist_entry(pos, struct trace_process, node);
			ptrace(PTRACE_DETACH, process->pid, NULL, NULL);
			trace_process_del(trace, process);
		}
	}
	free(trace);
}

int trace_continue(struct trace_process *process,
		   trace_callback callback, void *userdata) {
	process->callback = callback;
	process->userdata = userdata;
	return 0;
}

int trace_sfd(struct trace *trace) {
	return trace->sfd;
}

int trace_process_count(struct trace *trace) {
	return trace->process_count;
}


static int mem_fd_open(int pid) {
	char path[64];
	snprintf(path, sizeof(path), "/proc/%i/mem", pid);
	return open(path, O_RDONLY | O_CLOEXEC);
}

static struct trace_process *trace_process_new(struct trace *trace, int pid) {
	struct trace_process *process = calloc(1, sizeof(struct trace_process));
	process->pid = pid;
	process->mem_fd = mem_fd_open(pid);
	trace->process_count += 1;
	hlist_add_head(&process->node, &trace->hpids[pid % HPIDS_SIZE]);
	return process;
}

static void trace_process_del(struct trace *trace, struct trace_process *process) {
	hlist_del(&process->node);
	trace->process_count -= 1;
	if (process->mem_fd != -1)
		close(process->mem_fd);
	free(process);
}

#if 0
static void close_spare_fds() {
	DIR *d = opendir("/proc/self/fd");

	if (d) {
		struct dirent* de;
		while ((de = readdir(d))) {
			int i = atoi(de->d_name);
			if (i > 2) close(i);
		}
		closedir(d);
	} else {
		int i;
		for (i = 3; i < 256; i++) 
			close(i);
		return;
	}
}
#endif

int trace_execvp(struct trace *trace, char **argv) {
	int pid = fork();
	if (pid == -1)
		PFATAL("fork()");

	if (pid == 0) {
		// Make sure no descriptors leak (did you use O_CLOEXEC?)
		// Restore default sigprocmask
		sigset_t mask;
		sigemptyset(&mask);
		sigaddset(&mask, SIGCHLD);
		sigprocmask(SIG_UNBLOCK, &mask, NULL);

		int r = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		if (r < 0)
			PFATAL("ptrace(PTRACE_TRACEME)");
		// Wait for the parent to catch up.
		raise(SIGSTOP);
		
		execvp(argv[0], argv);
		PFATAL("execvp()");
	}

	struct trace_process *process = trace_process_new(trace, pid);
	/* On new process call trace->callback, not process->callback. */
	trace->callback(process, TRACE_ENTER, (void*)pid, trace->userdata);
	return pid;
}

static struct trace_process *process_by_pid(struct trace *trace, int pid) {
	struct trace_process *process = NULL;
	struct hlist_node *pos;
	hlist_for_each(pos, &trace->hpids[pid % HPIDS_SIZE]) {
		process = hlist_entry(pos, struct trace_process, node);
		if (process->pid == pid) {
			break;
		}
		process = NULL;
	}
	return process;
}

static void ptrace_prepare(int pid) {
	int r = ptrace(PTRACE_SETOPTIONS, pid, 0,
		       PTRACE_O_TRACESYSGOOD |
		       PTRACE_O_TRACEFORK |
		       PTRACE_O_TRACEVFORK |
		       PTRACE_O_TRACECLONE |
		       PTRACE_O_TRACEEXEC |
		       PTRACE_O_TRACEEXIT);
	if (r != 0)
		PFATAL("ptrace(PTRACE_SETOPTIONS)");
}

static int process_stoped(struct trace *trace, struct trace_process *process,
			  int signal) {

	int pid = process->pid;
	int inject_signal = 0;

	switch (signal) {

	case SIGTRAP | 0x80: { // assuming PTRACE_O_SYSGOOD
		REGS_STRUCT regs;
		if (ptrace(PTRACE_GETREGS, pid, 0, &regs) < 0)
			PFATAL("ptrace(PTRACE_GETREGS)");
		int syscall_entry = SYSCALL_ENTRY;
		struct trace_sysarg sysarg = {SYSCALL, ARG1, ARG2,
					      ARG3, ARG4, ARG5, ARG6, RET};
		process->regs = regs;
		if (syscall_entry != !process->within_syscall) {
			SHOUT("syscall entry - exit desynchronizaion");
		} else {
			int type = syscall_entry ? TRACE_SYSCALL_ENTER
						 : TRACE_SYSCALL_EXIT;
			process->callback(process, type, &sysarg,
					  process->userdata);
		}
		process->within_syscall = !process->within_syscall;
		break; }

	case SIGTRAP | (PTRACE_EVENT_FORK << 8):
	case SIGTRAP | (PTRACE_EVENT_VFORK << 8):
	case SIGTRAP | (PTRACE_EVENT_CLONE << 8): {
		unsigned long child_pid;
		if (ptrace(PTRACE_GETEVENTMSG, pid, NULL, &child_pid) < 0)
			PFATAL("ptrace(PTRACE_GETEVENTMSG)");
		struct trace_process *child_process =
			trace_process_new(trace, child_pid);
		trace->callback(child_process, TRACE_ENTER,
					 (void*)child_pid, trace->userdata);
		break; }

	case SIGTRAP | PTRACE_EVENT_EXEC << 8: {
		// /proc/<pid>/mem  must be re-opened after exec.
		if (process->mem_fd != -1)
			close(process->mem_fd);
		process->mem_fd = mem_fd_open(pid);
		break; }

	case SIGTRAP | PTRACE_EVENT_EXIT << 8:
		// exit() called, we'll see the process again during WIFEXITED()
		break;

	case SIGTRAP:
		/* Got a pure SIGTRAP - Why? Let's assume it's
		   just a normal signal. */
		// fall through...
	default:
		inject_signal = signal;
		process->callback(process, TRACE_SIGNAL, &inject_signal,
				  process->userdata);
		break;

	}

	return inject_signal;
}

static void process_evaluate(struct trace *trace,
			   struct trace_process *process, int status) {

	int inject_signal = 0;

	if (!process->initialized) {
		/* First child SIGSTOPs itself after calling TRACEME,
		   descendants are STOPPED due to TRACEFORK. */
		if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP) {
			PFATAL("Process in a wrong state! wifstopped=%i wsigstop=%i",
			       WIFSTOPPED(status), WSTOPSIG(status));
		}
		ptrace_prepare(process->pid);
		process->initialized = 1;
	} else {
		if (WIFSTOPPED(status)) {
			/* We can't use WSTOPSIG(status) - it cuts high bits. */
			int signal = (status >> 8) & 0xffff;
			inject_signal = process_stoped(trace, process, signal);
		} else
		if (WIFSIGNALED(status) || WIFEXITED(status)) {
			struct trace_exitarg exitarg;
			if (WIFSIGNALED(status)) {
				exitarg = (struct trace_exitarg){
					TRACE_EXIT_SIGNAL, WTERMSIG(status)};
			} else {
				exitarg = (struct trace_exitarg){
					TRACE_EXIT_NORMAL, WEXITSTATUS(status)};
			}
			process->callback(process, TRACE_EXIT, &exitarg,
					  process->userdata);
			trace_process_del(trace, process);
			return;
		} else {
			SHOUT("pid=%i status 0x%x not understood!", process->pid, status);
		}
	}


	int r = ptrace(PTRACE_SYSCALL, process->pid, 0, inject_signal);
	if (r < 0)
		PFATAL("ptrace(PTRACE_SYSCALL)");
}


/* Call this method when sfd is readable */
void trace_read(struct trace *trace) {
	struct signalfd_siginfo sinfo[4];

	int r = read(trace->sfd, &sinfo, sizeof(sinfo));
	if (r < 0)
		PFATAL("read(signal_fd)");

	if (r % sizeof(struct signalfd_siginfo) != 0)
		PFATAL("read(signal_fd) not aligned to signalfd_siginfo");

	if (r == 0)
		return;

	/* Although signalfd() socket is capable of buffering signals,
	 * losing one is still very much possible. Therefore it makes
	 * no sense to actually look at the results of read(). */

	// int sinfo_sz = r / sizeof(struct signalfd_siginfo);

	/* Unfortunately waitpid(*) is O(n), but by running
	 * waitpid(-1) we might starve processes with higher pids
	 * (further down the child list in the kernel). But this is
	 * not a big deal for us. */

	while (1) {
		int status;
		int pid = waitpid(-1, &status, WNOHANG | __WALL);
		if (pid == -1) {
			if (errno != ECHILD)
				PFATAL("waitpid()");
			break;
		}
		if (pid == 0) {
			break;
		}

		struct trace_process *process = process_by_pid(trace, pid);
		if (process) {
			process_evaluate(trace, process, status);
		} else {
			struct waitpid_report *sr =
				calloc(1, sizeof(struct waitpid_report));
			sr->pid = pid;
			sr->status = status;
			list_add(&sr->in_list, &trace->list_of_waitpid_reports);
		}
	}

	/* Process waitpids from processess we don't know only after
	 * all known processes were handled. This is required due to a
	 * race: we might get info from a new child process before
	 * parent tells us he did clone/fork. We end up in a report
	 * from an uknonwn process in such case. */
	struct list_head *pos, *tmp;
	list_for_each_safe(pos, tmp, &trace->list_of_waitpid_reports) {

		struct waitpid_report *sr =
			hlist_entry(pos, struct waitpid_report, in_list);

		list_del(&sr->in_list);
		struct trace_process *process = process_by_pid(trace, sr->pid);
		if (!process) {
			SHOUT("waitpid() returned unknown process pid=%i status=0x%x",
			      sr->pid, sr->status);
		} else {
			process_evaluate(trace, process, sr->status);
		}

		free(sr);
	}
}

void trace_setregs(struct trace_process *process, struct trace_sysarg *sysarg) {
	REGS_STRUCT regs = process->regs;
	SYSCALL = sysarg->number;
	ARG1 = sysarg->arg1;
	ARG2 = sysarg->arg2;
	ARG3 = sysarg->arg3;
	ARG4 = sysarg->arg4;
	ARG5 = sysarg->arg5;
	ARG6 = sysarg->arg6;
	RET = sysarg->ret;
	if (ptrace(PTRACE_SETREGS, process->pid, 0, &regs) < 0)
		PFATAL("ptrace(PTRACE_SETREGS)");
}

static int copy_from_user_ptrace(struct trace_process *process, void *dst,
				 unsigned long src, size_t len) {
	size_t words = len / sizeof(long);
	long *word_src = (long*)src;
	long *word_dst = dst;
	unsigned faults = 0;
	unsigned i;
	for (i = 0; i < words; i++) {
		if (errno)
			errno = 0;
		word_dst[i] = ptrace(PTRACE_PEEKDATA,  process->pid,
				     &word_src[i], NULL);
		if (errno) {
			if (errno == EIO || errno == EFAULT)
				faults += 1;
			else
				PFATAL("ptrace(PTRACE_PEEKDATA)");
		}
	}
	return faults * sizeof(long);
}

static int copy_to_user_ptrace(struct trace_process *process, unsigned long dst,
			       void *src, size_t len) {
	size_t words = len / sizeof(long);
	long *qword_src = src;
	long *qword_dst = (long*)dst;
	unsigned faults = 0;
	unsigned i;
	for (i = 0; i < words; i++) {
		int r = ptrace(PTRACE_POKEDATA,  process->pid,
			       &qword_dst[i], qword_src[i]);
		if (r == -1) {
			if (errno == EIO || errno == EFAULT)
				faults += 1;
			else
				PFATAL("ptrace(PTRACE_POKEDATA)");
		}
	}
	return faults * sizeof(long);
}

static int copy_from_user_fd(struct trace_process *process, void *dst,
			     unsigned long src, size_t len) {
	int r = pread(process->mem_fd, dst, len, src);
	if (r < 0)
		PFATAL("pread(\"/proc/%i/mem\", offset=0x%lx, len=%u) = %i",
		       process->pid, src, len, r);
	return len - (unsigned)r;
}

int copy_from_user(struct trace_process *process, void *dst,
		   unsigned long src, size_t len) {
	if (src % sizeof(long) || len % sizeof(long))
		PFATAL("unaligned");

	if (process->mem_fd != -1)
		return copy_from_user_fd(process, dst, src, len);
	return copy_from_user_ptrace(process, dst, src, len);
}

int copy_to_user(struct trace_process *process, unsigned long dst,
		 void *src, size_t len) {
	if (dst % sizeof(long) || len % sizeof(long))
		PFATAL("unaligned");

	return copy_to_user_ptrace(process, dst, src, len);
}
