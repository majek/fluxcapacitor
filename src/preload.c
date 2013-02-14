#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/syscall.h>

#include "types.h"

static int  (*libc_gettimeofday)(struct timeval *tv, struct timezone *tz);
static int (*libc_ftime)(struct timeb *tp);
static int (*libc_nanosleep)(const struct timespec *req, struct timespec *rem);

#define ERRORF(x...)  fprintf(stderr, x)
#define FATAL(x...) do {					\
		ERRORF("[-] PROGRAM ABORT : " x);		\
		ERRORF("\n         Location : %s(), %s:%u\n\n", \
		       __FUNCTION__, __FILE__, __LINE__);	\
		exit(EXIT_FAILURE);				\
	} while (0)

#if __GNUC__ >= 4
# define PUBLIC __attribute__ ((visibility ("default")))
# define LOCAL  __attribute__ ((visibility ("hidden")))
#else
# define PUBLIC
# define LOCAL
#endif

/* Not even attempting:
 *
 * alarm(2), ualarm(2), signalfd(2), timer_create(2),
 * timerfd_create(2), setitimer(2), rtc(4) */

#define TIMESPEC_NSEC(ts) ((ts)->tv_sec * 1000000000ULL + (ts)->tv_nsec)
#define NSEC_TIMESPEC(ns) (struct timespec){(ns) / 1000000000ULL, (ns) % 1000000000ULL}

PUBLIC
int clock_gettime(clockid_t clk_id, struct timespec *tp) {
	/* Ignore clk_id. Don't use libc version - we want a
	 * ptrace-able syscall not a vdso call. */
	return syscall(SYS_clock_gettime, CLOCK_REALTIME, tp);
}

/* Translate to clock_gettime() */
PUBLIC
int gettimeofday(struct timeval *tv, struct timezone *tz) {
	long a = 0, b = 0;
	if (tv) {
		struct timespec ts;
		a = syscall(SYS_clock_gettime, CLOCK_REALTIME, &ts);
		*tv = (struct timeval){ts.tv_sec, ts.tv_nsec / 1000ULL};
	}
	if (tz) {
		struct timeval tmp;
		b = libc_gettimeofday(&tmp, tz);
	}
	return a || b;
}

PUBLIC
time_t time(time_t *t) {
	struct timespec ts;
	syscall(SYS_clock_gettime, CLOCK_REALTIME, &ts);
	if (t) {
		*t = ts.tv_sec;
	}
	return ts.tv_sec;
}

PUBLIC
int ftime(struct timeb *tp) {
	if (tp) {
		libc_ftime(tp);
		struct timespec ts;
		syscall(SYS_clock_gettime, CLOCK_REALTIME, &ts);
		tp->time = ts.tv_sec;
		tp->millitm = ts.tv_nsec / 1000000ULL;
	}
	return 0;
}

PUBLIC
int nanosleep(const struct timespec *req, struct timespec *rem) {
	return clock_nanosleep(CLOCK_REALTIME, 0, req, rem);
}

PUBLIC
int clock_nanosleep(clockid_t clk_id, int flags,
		    const struct timespec *request,
		    struct timespec *remain) {
	/* As for clock_gettime() - ignore clk_id */
	struct timespec tmp;
	clock_gettime(CLOCK_REALTIME, &tmp);
	u64 now = TIMESPEC_NSEC(&tmp);
	u64 end, diff;
	if (flags & TIMER_ABSTIME) {
		end = TIMESPEC_NSEC(request);
		if (end > now) {
			diff = end - now;
		} else {
			diff = 0;
		}
	} else {
		end = now + TIMESPEC_NSEC(request);
		diff = TIMESPEC_NSEC(request);
	}
	tmp = NSEC_TIMESPEC(diff);
	int r = libc_nanosleep(&tmp, NULL);
	if (r == 0 && remain) {
		clock_gettime(CLOCK_REALTIME, &tmp);
		u64 t2 = TIMESPEC_NSEC(&tmp);
		if (end > t2) {
			*remain = NSEC_TIMESPEC(end - t2);
		} else {
			*remain = NSEC_TIMESPEC(0);
		}
	}
	return r;
}


static void __attribute__ ((constructor)) my_init(void)  {
	static void *libc_handle;
	libc_handle = dlopen("libc.so.6", RTLD_LAZY | RTLD_GLOBAL | RTLD_NOLOAD);
        if (!libc_handle) {
		FATAL("[-] dlopen(): %s", dlerror());
        }

	char *error;

	libc_gettimeofday = dlsym(libc_handle, "gettimeofday");
	error = dlerror();
	if (error != NULL) {
		FATAL("[-] dlsym(): %s", error);
	}

	libc_ftime = dlsym(libc_handle, "ftime");
	error = dlerror();
	if (error != NULL) {
		FATAL("[-] dlsym(): %s", error);
	}

	libc_nanosleep = dlsym(libc_handle, "nanosleep");
	error = dlerror();
	if (error != NULL) {
		FATAL("[-] dlsym(): %s", error);
	}
}
