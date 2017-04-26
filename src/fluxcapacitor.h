#define TEST_LIBNAME "fluxcapacitor_test.so"
#define PRELOAD_LIBNAME "fluxcapacitor_preload.so"

extern struct timespec uevent_now;

#define ERRORF(x...)  fprintf(stderr, x)
#define FATAL(x...) do {					\
		ERRORF("[-] PROGRAM ABORT : " x);		\
		ERRORF("\n         Location : %s(), %s:%u\n\n", \
		       __FUNCTION__, __FILE__, __LINE__);	\
		exit(EXIT_FAILURE);				\
	} while (0)

#define PFATAL(x...) do {                        \
		ERRORF("[-] SYSTEM ERROR : " x);		\
		ERRORF("\n        Location : %s(), %s:%u\n",    \
		       __FUNCTION__, __FILE__, __LINE__);       \
		perror("      OS message ");                    \
		ERRORF("\n");                                   \
		exit(127);                                      \
	} while (0)

#define SHOUT(x...) do{						\
		if (options.verbose) {				\
			fprintf(options.shoutstream, x);	\
			fprintf(options.shoutstream, "\n");	\
		}						\
	} while (0)

#define PRINT(x...) do{						\
		if (options.verbose > 1) {     			\
			fprintf(options.shoutstream, x);	\
			fprintf(options.shoutstream, "\n");	\
		}						\
	} while (0)

/* All the global state goes here */
struct options {
	/* Path to .so files */
	char *libpath;

	int verbose;

	FILE *shoutstream;

	/* Should we exit? */
	int exit_forced;

	/* Max exit status of the exited children (unsigned) */
	unsigned exit_status;

	/* Signo we'll use to continue the child. Must not be used by
	   the child application. */
	int signo;

	/* Wait for `idleness_threshold` ns of not handling any
	 * changes before speeding up time. */
	u64 idleness_threshold;

	/* Don't advance time in tiny chunks */
	u64 min_speedup;
};


struct parent {
	int child_count;
	struct list_head list_of_children;

	int blocked_count;
	struct list_head list_of_blocked;

	int started;

	s128 time_drift;
};


struct trace_process;

struct child {
	int pid;
	struct list_head in_children;
	struct list_head in_blocked;

	int blocked;
	s128 blocked_until;

	struct parent *parent;
	struct trace_process *process;

	int interrupted;

	int syscall_no;

	int stat_fd;
	char stat;
};


/* misc.c */
void pin_cpu();
char ***argv_split(char **argv, const char *delimiter, int upper_bound);
char *argv_join(char **argv, const char *delimiter);
void ensure_libpath(const char *argv_0);
void ldpreload_extend(const char *lib_path, const char *file);
const char *ldpreload_get();
void handle_backtrace();
int str_to_signal(const char *s);
int str_to_time(const char *s, u64 *timens_ptr);
const char *syscall_to_str(int no);
int proc_running();
void ping_myself();

/* parent.c */
#define TIMEOUT_UNKNOWN (-1LL)
/* 2**63 - 1, LLONG_MAX but not depending on limits.h */
#define	TIMEOUT_FOREVER (-2LL)

struct trace;
struct trace_process;
struct parent *parent_new();
void parent_run_one(struct parent *parent, struct trace *trace,
		    char **child_argv);
struct child *parent_min_timeout_child(struct parent *parent);
struct child *parent_woken_child(struct parent *parent);
void parent_kill_all(struct parent *parent, int signo);

struct child *child_new(struct parent *parent, struct trace_process *process, int pid);
void child_del(struct child *child);
struct trace_sysarg;
void child_mark_blocked(struct child *child);
void child_mark_unblocked(struct child *child);


void wrapper_syscall_enter(struct child *child, struct trace_sysarg *sysarg);
int wrapper_syscall_exit(struct child *child, struct trace_sysarg *sysarg);
void wrapper_pacify_signal(struct child *child, struct trace_sysarg *sysarg);



void child_kill(struct child *child, int signo);
void child_fake_response(struct child *child, struct trace_sysarg *sysarg);


#define TIMESPEC_NSEC(ts) ((ts)->tv_sec * 1000000000ULL + (ts)->tv_nsec)
#define TIMEVAL_NSEC(ts) ((ts)->tv_sec * 1000000000ULL + (ts)->tv_usec * 1000ULL)
#define NSEC_TIMESPEC(ns) (struct timespec){(ns) / 1000000000ULL, \
					    (ns) % 1000000000ULL}
#define NSEC_TIMEVAL(ns) (struct timeval){(ns) / 1000000000ULL, \
					 ((ns) % 1000000000ULL) / 1000ULL}
#define MSEC_NSEC(ms)   ((ms) * 1000000ULL)
