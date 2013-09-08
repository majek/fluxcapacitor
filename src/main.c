#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <sched.h>

#include "types.h"
#include "list.h"

#include "fluxcapacitor.h"
#include "uevent.h"
#include "trace.h"

static void usage() {
	ERRORF(
"Usage:\n"
"\n"
"    fluxcapacitor [options] [ -- command [ arguments ... ] ... ]\n"
"\n"
"Options:\n"
"\n"
"  --libpath=PATH       Load " PRELOAD_LIBNAME " from\n"
"                       selected PATH directory.\n"
"  --signal=SIGNAL      Use specified signal to interrupt blocking\n"
"                       syscall instead of SIGURG.\n"
"  --verbose,-v         Print more stuff. Repeat for debugging\n"
"                       messages.\n"
"  --help               Print this message.\n"
"\n"
		);
	exit(EXIT_FAILURE);
}



/* Global */
struct options options;

static u64 main_loop(char ***list_of_argv);

int main(int argc, char **argv) {

	options.verbose = 0;
	options.shoutstream = stderr;
	options.signo = SIGURG;

	handle_backtrace();
	pin_cpu();

	optind = 1;
	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"libpath",    required_argument, 0,  0  },
			{"help",       no_argument,       0, 'h' },
			{"verbose",    no_argument,       0, 'v' },
			{"signal",     required_argument, 0,  0  },
			{0,            0,                 0,  0  }
		};

		int arg = getopt_long(argc, argv, "vh",
				      long_options, &option_index);
		if (arg == -1) {
			break;
		}

		switch (arg) {
		case 0: {
			const char *opt_name = long_options[option_index].name;
			if (0 == strcasecmp(opt_name, "libpath")) {
				options.libpath = strdup(optarg);
			} else if (0 == strcasecmp(opt_name, "signal")) {
				options.signo = str_to_signal(optarg);
				if (!options.signo)
					FATAL("Unrecognised signal \"%s\"", optarg);
			} else {
				FATAL("Unknown option: %s", argv[optind]);
			}
			break; }

		case 'v':
			options.verbose += 1;
			break;

		case 'h':
			usage();
			break;

		default:
			FATAL("Unknown option: %s", argv[optind]);
		}
	}

	if (!argv[optind]) {
		FATAL("You must specify at least one command to execute.");
	}

	char ***list_of_argv = argv_split(&argv[optind], "--", argc);

	ensure_libpath(argv[0]);
	ldpreload_extend(options.libpath, PRELOAD_LIBNAME);

	SHOUT("--- Flux Capacitor ---\n");

	SHOUT("[.] LD_PRELOAD=%s", ldpreload_get());

	u64 time_drift = main_loop(list_of_argv);

	free(options.libpath);
	fflush(options.shoutstream);
	char ***child_argv = list_of_argv;
	while (*child_argv) {
		free(*child_argv);
		child_argv ++;
	}
	free(list_of_argv);

	PRINT(" ~  Exiting with code %i. Speedup %.3f sec.",
	      options.exit_status, time_drift / 1000000000.);
	return options.exit_status;
}


static int on_signal(struct uevent *uevent, int sfd, int mask, void *userdata) {
	struct trace *trace = userdata;
	trace_read(trace);
	return 0;
}


static int on_trace(struct trace_process *process, int type, void *arg,
		    void *userdata);

static int on_trace_start(struct trace_process *process, int type, void *arg,
			  void *userdata) {
	if (type != TRACE_ENTER)
		FATAL("");
	int pid = (long)arg;
	SHOUT("[+] %i started", pid);
	struct parent *parent = (struct parent *)userdata;
	struct child *child = child_new(parent, process, pid);
	return trace_continue(process, on_trace, child);
}

static int on_trace(struct trace_process *process, int type, void *arg,
		    void *userdata) {
	struct child *child = userdata;
	switch (type) {

	case TRACE_EXIT: {
		struct trace_exitarg *exitarg = arg;
		if (exitarg->type == TRACE_EXIT_NORMAL) {
			SHOUT("[-] %i exited with return status %u",
			      child->pid, exitarg->value);
			options.exit_status = MAX(options.exit_status,
						  (unsigned)exitarg->value);
		} else {
			SHOUT("[-] %i exited due to signal %u",
			      child->pid, exitarg->value);
		}
		child_del(child);
		break; }

	case TRACE_SYSCALL_ENTER: {
		struct trace_sysarg *sysarg = arg;
		child_mark_blocked(child);
		wrapper_syscall_enter(child, sysarg);
		break; }

	case TRACE_SYSCALL_EXIT: {
		struct trace_sysarg *sysarg = arg;
		child_mark_unblocked(child);
		wrapper_syscall_exit(child, sysarg);
		if (child->interrupted) {
			child->interrupted = 0;
			wrapper_pacify_signal(child, sysarg);
		}
		break; }

	case TRACE_SIGNAL: {
		int *signal_ptr = (int*)arg;
		if (*signal_ptr == options.signo)
			*signal_ptr = 0;
		break; }

	default:
		FATAL("");

	}
	return 0;
}

static u64 main_loop(char ***list_of_argv) {
	struct timeval timeout;

	struct parent *parent = parent_new();
	struct trace *trace = trace_new(on_trace_start, parent);
	struct uevent *uevent = uevent_new(NULL);

	parent_run_one(parent, trace, *list_of_argv);
	list_of_argv ++;

	uevent_yield(uevent, trace_sfd(trace), UEVENT_READ, on_signal, trace);

	while ((parent->child_count || *list_of_argv) && !options.exit_forced) {
		/* Is everyone blocking? */
		if (parent->blocked_count != parent->child_count) {
			/* Nope, need to wait for some process to block */
			uevent_select(uevent, NULL);
			continue;
		}

		/* Continue only after some time passed with no action. */
		if (parent->child_count) {
			/* Say a child process did a syscall that
			 * produces side effects. For example a
			 * network write. It make take a while before
			 * the side effects become visible to another
			 * watched process.
			 *
			 * Although from our point of view everyone's
			 * "blocked", there may be some stuff
			 * available but not yet processed by the
			 * kernel. We must give some time for a kernel
			 * to work it out.  */

			/* First. Let's make it clear we want to give
			 * priority to anybody requiring CPU now. */
			sched_yield();
			sched_yield();

#if 0
			/* Next, let's wait until we're the only
			 * process in running state. This can be
			 * painful on SMP.
			 *
			 * This also means fluxcapacitor won't work on
			 * a busy system. */
			if (proc_running() > 1) {
				int c = 0;
				for (c = 0; c < 3 * parent->child_count; c++) {
					if (proc_running() < 2)
						break;
					sched_yield();
				}

				SHOUT("[ ] Your system looks busy. I waited %i sched_yields.", c);
			}
#endif

			/* Now, lets wait for 1us to see if anything
			 * new arrived. Setting timeout to zero
			 * doesn't work - kernel returns immediately
			 * and doesn't do any work. Therefore we must
			 * set the timeout to a next smallest value,
			 * and 'select()' granularity is in us. */

			/* Let's make it 1ms. */
			timeout = NSEC_TIMEVAL(1000000ULL); // 1ms
			int r = uevent_select(uevent, &timeout);
			if (r != 0)
				continue;

			/* Finally, make sure all processes are in 'S'
			 * sleeping state. They should be! */
			struct child *woken = parent_woken_child(parent);
			if (woken) {
				SHOUT("[ ] %i Process not in 'S' state",
				      woken->pid);
				timeout = NSEC_TIMEVAL(10 * 1000000ULL); // 10ms
				int r = uevent_select(uevent, &timeout);
				if (r == 0)
					SHOUT("[ ] Waited for 10ms and nothing happened");
				continue;
			}
		}

		/* All childs started? */
		if (*list_of_argv) {
			parent_run_one(parent, trace, *list_of_argv);
			list_of_argv ++;
			uevent_select(uevent, NULL);
			continue;
		}

		/* Hurray, we're most likely waiting for a timeout. */
		struct child *min_child = parent_min_timeout_child(parent);
		if (min_child) {
			s64 now = TIMESPEC_NSEC(&uevent_now) + parent->time_drift;
			s64 speedup = min_child->blocked_until - now;
			if (speedup > 0) {
				SHOUT("[ ] %i speeding up %s() by %.3f sec",
				      min_child->pid,
				      syscall_to_str(min_child->syscall_no),
				      speedup / 1000000000.0);
			} else {
				/* Timeout already passed, wake up the process */
				speedup = 0;
				SHOUT("[ ] %i waking expired %s()",
				      min_child->pid,
				      syscall_to_str(min_child->syscall_no));
			}
			parent->time_drift += speedup;
			min_child->interrupted = 1;
			child_kill(min_child, options.signo);
		} else {
			SHOUT(" ! Can't speedup!");
			/* Wait for any event. */
			uevent_select(uevent, NULL);
		}
	}
	parent_kill_all(parent, SIGINT);

	trace_free(trace);

	u64 time_drift = parent->time_drift;
	free(parent);
	free(uevent);

	return time_drift;
}
