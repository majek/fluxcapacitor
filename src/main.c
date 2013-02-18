#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
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
"  --idleness=TIMEOUT   Speed up time only when all processess were\n"
"                       idle for more than TIMEOUT (50ms by default).\n"
"  --verbose,-v         Print more stuff.\n"
"  --help               Print this message.\n"
"\n"
		);
	exit(EXIT_FAILURE);
}



/* Global */
struct options options;

static void main_loop(char ***list_of_argv);


int main(int argc, char **argv) {

	options.quiet = 1;
	options.shoutstream = stderr;
	options.signo = SIGURG;
	options.idleness_threshold = 50 * 1000000ULL; /* 50ms */

	handle_backtrace();
	pin_cpu();

	optind = 1;
	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"libpath",  required_argument, 0,  0  },
			{"help",     no_argument,       0, 'h' },
			{"verbose",  no_argument,       0, 'v' },
			{"signal",   required_argument, 0,  0  },
			{"idleness", required_argument, 0,  0  },
			{0,          0,                 0,  0  }
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
			} else if (0 == strcasecmp(opt_name, "idleness")) {
				options.idleness_threshold = str_to_time(optarg);
				if (!options.idleness_threshold)
					FATAL("Wrong TIMEOUT \"%s\".", optarg);
			} else {
				FATAL("Unknown option: %s", argv[optind]);
			}
			break; }

		case 'v':
			options.quiet = 0;
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

	main_loop(list_of_argv);

	free(options.libpath);
	fflush(options.shoutstream);
	char ***child_argv = list_of_argv;
	while (*child_argv) {
		free(*child_argv);
		child_argv ++;
	}
	free(list_of_argv);
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
	int pid = (int)arg;
	SHOUT("[+] pid=%i started", pid);
	struct parent *parent = (struct parent *)userdata;
	struct child *child = child_new(parent, process, pid);
	return trace_continue(process, on_trace, child);
}

static int on_trace(struct trace_process *process, int type, void *arg,
		    void *userdata) {
	struct child *child = userdata;
	switch (type) {

	case TRACE_EXIT:
		SHOUT("[-] pid=%i exited", child->pid);
		child_del(child);
		return 0;

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

static void main_loop(char ***list_of_argv) {
	int r;
	struct timeval timeout;

	struct parent *parent = parent_new();
	struct trace *trace = trace_new(on_trace_start, parent);
	struct uevent *uevent = uevent_new(NULL);

	parent_run_one(parent, trace, *list_of_argv);
	list_of_argv ++;

	uevent_yield(uevent, trace_sfd(trace), UEVENT_READ, on_signal, trace);

	while ((parent->child_count || *list_of_argv) && !options.exit_forced) {
		/* Continue only after 1 ms of idleness */
		timeout = (struct timeval){0, 1};
		r = uevent_select(uevent, &timeout);
		if (r != 0)
			continue;

		/* If a scheduled syscall is to finish quickly, give
		 * it time to do so. sched_yield() may not be ideal in
		 * newer linux but it won't hurt. */
		sched_yield();

		/* Continue only after some time passed with no action. */
		if (!parent->child_count)
			break;
		timeout = NSEC_TIMEVAL(options.idleness_threshold);
		r = uevent_select(uevent, &timeout);
		if (r != 0)
			continue;

		/* All childs started? */
		if (*list_of_argv) {
			parent_run_one(parent, trace, *list_of_argv);
			list_of_argv ++;
			if (!parent->child_count)
				break;
			uevent_select(uevent, NULL);
			continue;
		}

		/* Everyone's blocking? */
		if (parent->blocked_count != parent->child_count) {
			/* Need to wait for some process to block */
			if (!parent->child_count)
				break;
			uevent_select(uevent, NULL);
			continue;
		}


		/* Hurray, we're most likely waiting for a timeout. */
		struct child *min_child = parent_maybe_speedup(parent);
		static int done = 0;
		//SHOUT("everyone's blocking! mbsp=%i", r);
		if (min_child && !done) {
			u64 speedup = min_child->blocked_timeout;
			if (speedup < 10*1000000ULL) {
				/* nah, let's just wait */
				if (!parent->child_count)
					break;
				uevent_select(uevent, NULL);
				continue;
			}
			parent->time_drift += speedup;
			done = 0;
			SHOUT("[ ] Pid=%i speeding up %s() by %.3f sec",
			      min_child->pid,
			      syscall_to_str(min_child->syscall_no),
			      speedup / 1000000000.0);
			min_child->interrupted = 1;
			child_kill(min_child, options.signo);
		}
	}
	parent_kill_all(parent, SIGINT);

	trace_free(trace);
}
