#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <linux/futex.h>
#include <sys/syscall.h>
#include <errno.h>
#include <time.h>

#include "list.h"
#include "types.h"
#include "trace.h"
#include "fluxcapacitor.h"


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
	int value = 0;

	switch ((unsigned short)sysarg->number) {
	case SYS_epoll_wait:
	case SYS_epoll_pwait:
		type = TYPE_MSEC;value = sysarg->arg4; break;

	case SYS__newselect:
		type = TYPE_TIMEVAL; value = sysarg->arg5; break;
	case SYS_pselect6:
		type = TYPE_TIMESPEC; value = sysarg->arg5; break;

	case SYS_poll:
		type = TYPE_MSEC; value = sysarg->arg3; break;
	case SYS_ppoll:
		type = TYPE_TIMESPEC; value = sysarg->arg3; break;

	case SYS_clock_nanosleep:
		FATAL("clock_nanosleep() unsupported");

	case SYS_nanosleep:
		/* Second argument to nanosleep() can be ignored, it's
		   only supposed to be set on EINTR. */
		type = TYPE_TIMESPEC; value = sysarg->arg1;
		break;

	case SYS_futex:
		if (sysarg->arg2 == FUTEX_WAIT || sysarg->arg2 == FUTEX_WAIT_PRIVATE) {
			type = TYPE_TIMESPEC; value = sysarg->arg4;
		}
		break;
	}

	s64 timeout = TIMEOUT_UNKNOWN;
	switch (type) {
	case 0: break;

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

	child->blocked_timeout = timeout;
	child->syscall_no = sysarg->number;

}

int wrapper_syscall_exit(struct child *child, struct trace_sysarg *sysarg) {

	child->syscall_no = 0;

	switch (sysarg->number) {

	case SYS_clock_gettime:{
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
	switch (sysarg->number) {
	case SYS_epoll_wait:
	case SYS_epoll_pwait:
		if (sysarg->ret == -EINTR) {
			sysarg->ret = 0;
		}
	case SYS__newselect:
	case SYS_nanosleep:
		if (-512 >= sysarg->ret && sysarg->ret >= -517) { // ERESTART_RESTARTBLOCK
			sysarg->ret = 0;
		}
	case SYS_pselect6:
	case SYS_poll:
		if (-516 == sysarg->ret) {
			sysarg->ret = 0;
		}
	case SYS_ppoll:
	case SYS_futex:		/* should be ETIMEOUT */
		if (sysarg->ret != 0) {
			fprintf(stderr, "can't fake resp %s=%li\n",
				syscall_to_str(sysarg->number),
				sysarg->ret);
			return;
		}
		break;
	default:
		return;
	}
	trace_setregs(child->process, sysarg);
}
