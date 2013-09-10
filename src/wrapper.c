#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/prctl.h>

#include "list.h"
#include "types.h"
#include "trace.h"
#include "fluxcapacitor.h"
#include "scnums.h"

extern struct options options;


enum {
	TYPE_MSEC = 1,
	TYPE_TIMEVAL,
	TYPE_TIMESPEC,
	TYPE_FOREVER
};

/* Responsibilities:
 *  - save child->blocked_time if syscall is recognized
 *  - work together with preload.c to simplify syscall parameters
 */
void wrapper_syscall_enter(struct child *child, struct trace_sysarg *sysarg) {

	int type = 0;
	long value = 0;

	switch ((unsigned short)sysarg->number) {
	case __NR_epoll_wait:
	case __NR_epoll_pwait:
		type = TYPE_MSEC; value = sysarg->arg4; break;

#ifdef __NR_select
	case __NR_select:
#endif
#ifdef __NR__newselect
	case __NR__newselect:
		type = TYPE_TIMEVAL; value = sysarg->arg5; break;
#endif
	case __NR_pselect6:
		type = TYPE_TIMESPEC; value = sysarg->arg5; break;

	case __NR_poll:
		type = TYPE_MSEC; value = sysarg->arg3; break;
	case __NR_ppoll:
		type = TYPE_TIMESPEC; value = sysarg->arg3; break;

	case __NR_clock_nanosleep:
		FATAL("clock_nanosleep() unsupported");

	case __NR_nanosleep:
		/* Second argument to nanosleep() can be ignored, it's
		   only supposed to be set on EINTR. */
		type = TYPE_TIMESPEC; value = sysarg->arg1;
		break;

	/* Anti-debugging machinery. Prevent processes from disabling ptrace. */
	case __NR_prctl:
		if (sysarg->arg1 == PR_SET_DUMPABLE && sysarg->arg2 == 0) {
			SHOUT("[ ] %i pacyfying prctl(PR_SET_DUMPABLE, 0)",
			      child->pid);
			sysarg->arg2 = 1;
			trace_setregs(child->process, sysarg);
		}
		break;
	}

	if (!type)
		return;

	s64 timeout = TIMEOUT_UNKNOWN;
	switch (type) {
	case TYPE_MSEC:
		if (value < 0) {
			timeout = TIMEOUT_FOREVER;
		} else {
			timeout = MSEC_NSEC(value);
		}
		break;

	case TYPE_TIMEVAL:
		if (value == 0 ) { /* NULL */
			timeout = TIMEOUT_FOREVER;
		} else {
			struct timeval tv;
			copy_from_user(child->process, &tv, value, sizeof(struct timeval));
			timeout = TIMEVAL_NSEC(&tv);
		}
		break;

	case TYPE_TIMESPEC:
		if (value == 0) { /* NULL */
			timeout = TIMEOUT_FOREVER;
		} else {
			struct timespec ts;
			copy_from_user(child->process, &ts, value, sizeof(struct timespec));
			timeout = TIMESPEC_NSEC(&ts);
		}
		break;

	case TYPE_FOREVER:
		timeout = TIMEOUT_FOREVER;
		break;

	default:
		FATAL("");
	}

	switch (timeout) {
	case TIMEOUT_UNKNOWN:
	case TIMEOUT_FOREVER:
		PRINT(" ~  %i blocking on %s() %s",
		      child->pid, syscall_to_str(sysarg->number),
		      timeout == TIMEOUT_FOREVER ? "forever" : "unknow timeout");
		child->blocked_until = timeout;
		break;
	default:
		PRINT(" ~  %i blocking on %s() for %.3f sec",
		      child->pid, syscall_to_str(sysarg->number),
		      timeout / 1000000000.);
		child->blocked_until = (TIMESPEC_NSEC(&uevent_now) +
					child->parent->time_drift + timeout);
	}
	child->syscall_no = sysarg->number;
}

int wrapper_syscall_exit(struct child *child, struct trace_sysarg *sysarg) {

	child->syscall_no = 0;

	switch (sysarg->number) {

	case __NR_clock_gettime: {
		if (sysarg->ret == 0) {
			if (sysarg->arg1 != CLOCK_REALTIME && sysarg->arg1 != CLOCK_MONOTONIC)
				FATAL("%li ", sysarg->arg1);
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			u64 newtime = TIMESPEC_NSEC(&ts) + child->parent->time_drift;
			ts = NSEC_TIMESPEC(newtime);
			copy_to_user(child->process, sysarg->arg2, &ts,
				     sizeof(struct timespec));
		}
		break;}

	}
	return 0;

}


/* When we break a syscall by sending a signal, kernel returns
 * ERESTART_RESTARTBLOCK or similar error. Here we rewrite this value
 * to a result that will read: "timeout". */
void wrapper_pacify_signal(struct child *child, struct trace_sysarg *sysarg) {

	long orig_ret = sysarg->ret;

	switch (sysarg->number) {
	case __NR_epoll_wait:
	case __NR_epoll_pwait:
#ifdef __NR__newselect
	case __NR__newselect:
#endif
#ifdef __NR_select
	case __NR_select:
#endif
	case __NR_nanosleep:
	case __NR_pselect6:
	case __NR_poll:
	case __NR_ppoll:
		if (sysarg->ret == -EINTR) {
			sysarg->ret = 0;
		}
		if (-512 >= sysarg->ret && sysarg->ret >= -517) { // ERESTART_RESTARTBLOCK
			sysarg->ret = 0;
		}
		break;
	default:
		return;
	}
	PRINT(" ~  %i restarting %s(). kernel returned %li, changed to %li",
	      child->pid, syscall_to_str(sysarg->number), orig_ret, sysarg->ret);

	trace_setregs(child->process, sysarg);
}
