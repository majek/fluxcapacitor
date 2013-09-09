#define _GNU_SOURCE   /* sched_setaffinity() and sched_getcpu() */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <execinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <libgen.h>
#include <dlfcn.h>

#include "types.h"
#include "list.h"

#include "fluxcapacitor.h"
#include "scnums.h"


extern struct options options;


/* Pin current process and its children to a single CPU. Man
 * sched_setaffinity(2) says:
 *
 * > A child created via fork(2) inherits its parent's CPU affinity
 * > mask.  The affinity mask is preserved across an execve(2).
 */
void pin_cpu() {
	int cpu = sched_getcpu();
	if (cpu == -1)
		PFATAL("sched_getcpu()");

	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);
	int r = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
	if (r == -1)
		PFATAL("sched_setaffinity()");
}

char ***argv_split(char **argv, const char *delimiter, int upper_bound) {
	upper_bound += 1;

	int child_no = 0;
	char ***child_argv = malloc(upper_bound * sizeof(char *));

	while (*argv) {
		int pos = 0;
		child_argv[child_no] = malloc(upper_bound * sizeof(char *));
		for (;*argv; argv++) {
			if (strcmp(*argv, delimiter) != 0) {
				child_argv[child_no][pos++] = *argv;
			} else {
				argv ++;
				break;
			}
		}
		child_argv[child_no][pos++] = NULL;
		child_argv[child_no] = realloc(child_argv[child_no],
					       pos * sizeof(char *));
		child_no += 1;
	}
	child_argv[child_no] = NULL;
	child_no += 1;
	return realloc(child_argv, child_no * sizeof(char *));
}


/* Returns malloced memory */
char *argv_join(char **argv, const char *delim) {
	int len = 0, delim_len = strlen(delim);
	char **a;
	for (a = argv; *a; a++) {
		len += strlen(*a) + delim_len;
	}
	if (len) len -= delim_len;
	char *s = malloc(len + 1), *p = s;
	for (a = argv; *a; a++) {
		if (a != argv)
			p = stpcpy(p, delim);
		p = stpcpy(p, *a);
	}
	*p = '\0';
	return s;
}

#define PATH_DELIMITER "/"

static int dl_checkpath(const char *path_prefix, const char *solib) {
	char filename[PATH_MAX];
	if (strlen(path_prefix) > 0) {
		snprintf(filename, sizeof(filename), "%s" PATH_DELIMITER "%s",
			 path_prefix, solib);
	} else {
		snprintf(filename, sizeof(filename), "%s", solib);
	}

	void *handle = dlopen(filename, RTLD_LAZY | RTLD_LOCAL);
	if (!handle) {
		PRINT(" ~  Trying dlopen(\"%s\"): fail", filename);
		return 0;
	} else {
		PRINT(" ~  Trying dlopen(\"%s\"): success", filename);
		dlclose(handle);
		return 1;
	}
}


/* Make sure options.libpath is actually working. */
void ensure_libpath(const char *argv_0) {
	if (!options.libpath) {
		char *path = NULL;
		do {
			// 1. Relative to executable, useful for development
			char tmp[PATH_MAX];
			if (!realpath(argv_0, tmp)) {
				PFATAL("realpath(argv[0])");
			}
			path = dirname(tmp);
			if (dl_checkpath(path, TEST_LIBNAME)) break;

			// 2. Linker resolution (ie: no slash in name)
			path = "";
			if (dl_checkpath(path, TEST_LIBNAME)) break;

			// 3. Give up.
			path = NULL;
		} while(0);

		if (!path) {
			FATAL("Unable to load library \"" PRELOAD_LIBNAME "\", "
			      "most likely I tried a wrong path. "
			      "Consider specifying --libpath option.");
		}
		options.libpath = strdup(path);
	} else {
		if (!dl_checkpath(options.libpath, TEST_LIBNAME)) {
			FATAL("Unable to load " PRELOAD_LIBNAME " from directory \"%s\".\n"
			      "\tAre files \"" PRELOAD_LIBNAME "\" and \"" TEST_LIBNAME "\" available there?", options.libpath);
		}
	}
}

void ldpreload_extend(const char *lib_path, const char *file) {
        char pathname[PATH_MAX];
        if (strlen(lib_path) > 0) {
                snprintf(pathname, sizeof(pathname), "%s" PATH_DELIMITER "%s",
			 lib_path, file);
        } else {
                snprintf(pathname, sizeof(pathname), "%s", file);
        }

        char ld_preload[PATH_MAX];
        char *prev_ld_preload = getenv("LD_PRELOAD");

        if (prev_ld_preload == NULL) {
                snprintf(ld_preload, sizeof(ld_preload), "%s", pathname);
        } else {
                // On Linux LD_PRELOAD is whitespace separated, with first
                // library taking precedence
                snprintf(ld_preload, sizeof(ld_preload), "%s %s",
			 pathname, prev_ld_preload);
        }
        setenv("LD_PRELOAD", ld_preload, 1);

}

const char *ldpreload_get() {
        return getenv("LD_PRELOAD");
}


static void *getMcontextEip(ucontext_t *uc) {
#if defined(__APPLE__) && !defined(MAC_OS_X_VERSION_10_6)
	/* OSX < 10.6 */
#if defined(__x86_64__)
	return (void*) uc->uc_mcontext->__ss.__rip;
#elif defined(__i386__)
	return (void*) uc->uc_mcontext->__ss.__eip;
#else
	return (void*) uc->uc_mcontext->__ss.__srr0;
#endif
#elif defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_6)
	/* OSX >= 10.6 */
#if defined(_STRUCT_X86_THREAD_STATE64) && !defined(__i386__)
	return (void*) uc->uc_mcontext->__ss.__rip;
#else
	return (void*) uc->uc_mcontext->__ss.__eip;
#endif
#elif defined(__linux__)
	/* Linux */
#if defined(__i386__)
	return (void*) uc->uc_mcontext.gregs[14]; /* Linux 32 */
#elif defined(__X86_64__) || defined(__x86_64__)
	return (void*) uc->uc_mcontext.gregs[16]; /* Linux 64 */
#elif defined(__ia64__) /* Linux IA64 */
	return (void*) uc->uc_mcontext.sc_ip;
#endif
#else
	return NULL;
#endif
}

/* TODO: replace with libunwind one day. */
static void sigsegvHandler(int sig, siginfo_t *info, void *secret) {
	ucontext_t *uc = (ucontext_t*) secret;
	fprintf(stderr, "Crashed with signal %i\n", sig);

	void *trace[100];
	int trace_size = 0;

	/* Generate the stack trace */
	trace_size = backtrace(trace, 100);

	/* overwrite sigaction with caller's address */
	if (getMcontextEip(uc) != NULL)
		trace[1] = getMcontextEip(uc);

	/* Write symbols to log file */
	backtrace_symbols_fd(trace, trace_size, 2); /* stderr */
}

void handle_backtrace() {
	struct sigaction act;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
	act.sa_sigaction = sigsegvHandler;
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGILL, &act, NULL);
}


static struct {
	int no;
	char *name;
} signal_names[] = {
	{SIGHUP, "SIGHUP"},
	{SIGINT, "SIGINT"},
	{SIGQUIT, "SIGQUIT"},
	{SIGILL, "SIGILL"},
	{SIGTRAP, "SIGTRAP"},
	{SIGABRT, "SIGABRT"},
	{SIGIOT, "SIGIOT"},
	{SIGBUS, "SIGBUS"},
	{SIGFPE, "SIGFPE"},
	{SIGKILL, "SIGKILL"},
	{SIGUSR1, "SIGUSR1"},
	{SIGSEGV, "SIGSEGV"},
	{SIGUSR2, "SIGUSR2"},
	{SIGPIPE, "SIGPIPE"},
	{SIGALRM, "SIGALRM"},
	{SIGTERM, "SIGTERM"},
	{SIGSTKFLT, "SIGSTKFLT"},
	{SIGCHLD, "SIGCHLD"},
	{SIGCLD, "SIGCLD"},
	{SIGCONT, "SIGCONT"},
	{SIGSTOP, "SIGSTOP"},
	{SIGTSTP, "SIGTSTP"},
	{SIGTTIN, "SIGTTIN"},
	{SIGTTOU, "SIGTTOU"},
	{SIGURG, "SIGURG"},
	{SIGXCPU, "SIGXCPU"},
	{SIGXFSZ, "SIGXFSZ"},
	{SIGVTALRM, "SIGVTALRM"},
	{SIGPROF, "SIGPROF"},
	{SIGWINCH, "SIGWINCH"},
	{SIGIO, "SIGIO"},
	{SIGPOLL, "SIGPOLL"},
	{SIGPWR, "SIGPWR"},
	{SIGSYS, "SIGSYS"},
	{0, NULL}
};


int str_to_signal(const char *s) {

	int signo = atoi(s);
	if (signo > 0 && signo < SIGRTMAX)
		return signo;
	int i;
	for (i=0; signal_names[i].no; i++) {
		if (strcasecmp(s, signal_names[i].name) == 0 ||
		    strcasecmp(s, &signal_names[i].name[3]) == 0) {
			return signal_names[i].no;
		}
	}

	int rtoffset = 0;
	if (strncasecmp(s, "SIGRTMIN+", 9) == 0)
		rtoffset = 9;
	if (strncasecmp(s, "RTMIN+", 6) == 0)
		rtoffset = 6;
	if (rtoffset) {
		signo = SIGRTMIN + atoi(&s[rtoffset]);
		if (signo > 0 && signo < SIGRTMAX)
			return signo;
	}
	return 0;

}


int str_to_time(const char *s, u64 *timens_ptr) {
	char *end;
	u64 v = strtoull(s, &end, 10);

	if (end == s)
		return -1;

	if (*end == '\0' ||
	    strcasecmp(end, "ns") == 0 ||
	    strcasecmp(end, "nsec") == 0) {
		// already in ns
	} else if (strcasecmp(end, "us") == 0 || strcasecmp(end, "usec") == 0) {
		v *= 1000ULL;
	} else if (strcasecmp(end, "ms") == 0 || strcasecmp(end, "msec") == 0) {
		v *= 1000000ULL;
	} else if (strcasecmp(end, "s") == 0 || strcasecmp(end, "sec") == 0) {
		v *= 1000000000ULL;
	} else {
		return -1;
	}

	*timens_ptr = v;
	return 0;
}

const char *syscall_to_str(int no) {
	static char buf[32];
	const char *r = NULL;
	const int map_sz = sizeof(syscall_to_str_map) / sizeof(syscall_to_str_map[0]);
	if (no >= 0 && no < map_sz) {
		r = syscall_to_str_map[no];
	}
	if (!r) {
		snprintf(buf, sizeof(buf), "syscall_%i", no);
		r = buf;
	}
	return r;
}

int proc_running() {
	static int fd = -1;
	if (fd == -1) {
		fd = open("/proc/stat", O_RDONLY | O_CLOEXEC);
	}

	char buf[1024*16];
	int r = pread(fd, buf, sizeof(buf)-1, 0);
	if (r < 16)
		FATAL("");
	buf[r] = '\0';

	char *start, *saveptr;
	for (start = buf; ; start = NULL) {
		char *line = strtok_r(start, "\n", &saveptr);
		if (line == NULL)
			break;
		if (strncmp(line, "procs_running ", 14) == 0) {
			return atoi(line + 14);
		}
	}
	FATAL("Can't read /proc/stat!");
	return -1;
}

void ping_myself() {
	static int cd = -1;
	static int rd = -1;

	if (cd == -1) {
		int sd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
		if (sd < 0)
			PFATAL("socket()");
		int one = 1;
		int r = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *) &one,
				   sizeof(one));
		if (r < 0)
			PFATAL("setsockopt()");

		struct sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		sin.sin_port = 0; // assign any free
		if (bind(sd, (void *)&sin, sizeof(sin)) < 0)
			PFATAL("bind");

		r = listen(sd, 16);
		if (r < 0)
			PFATAL("listen()");

		struct sockaddr_in bind_sin;
		socklen_t bind_sin_len = sizeof(bind_sin);
		r = getsockname(sd, &bind_sin, &bind_sin_len);
		if (r < 0)
			PFATAL("getsockname()");
		if (bind_sin_len != sizeof(bind_sin))
			FATAL("getsockname length");


		cd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
		if (cd < 0)
			PFATAL("socket()");

		r = connect(cd, &bind_sin, sizeof(bind_sin));
		if (r < 0)
			PFATAL("connect()");

		rd = accept(sd, NULL, NULL);
		if (rd < 0)
			PFATAL("accept()");

		if (fcntl(rd, F_SETFD, FD_CLOEXEC) == -1)
			PFATAL("fcntl(FD_CLOEXEC)");

		/* Don't need bind socket anymore */
		close(sd);
	}

	char buf[128];

	int r = write(cd, buf, sizeof(buf));
	if (r != 128)
		PFATAL("write()");

	r = read(rd, buf, sizeof(buf));
	if (r != 128)
		PFATAL("read()");

}
