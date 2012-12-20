enum trace_types {
	TRACE_ENTER,		/* arg = (void*)pid */
	TRACE_EXIT,		/* arg = ptr to trace_exitarg */
	TRACE_SYSCALL_ENTER,	/* arg = ptr to trace_sysarg */
	TRACE_SYSCALL_EXIT,	/* arg = ptr to trace_sysarg */
	TRACE_SIGNAL		/* arg = ptr to signal number */
};

enum {
	TRACE_EXIT_NORMAL,
	TRACE_EXIT_SIGNAL
};

struct trace_exitarg {
	int type;		/* normal exit or signal */
	int value;		/* signal no or exit status */
};

struct trace_sysarg {
	int number;
	long arg1;
	long arg2;
	long arg3;
	long arg4;
	long arg5;
	long arg6;
	long ret;
};


struct trace;
struct trace_process;
typedef int (*trace_callback)(struct trace_process *process,
			      int type, void *arg, void *userdata);

/* Allocate and initialize `struct trace`. `callback` with `tracedata`
 * will be called on the arrival of a new process. */
struct trace *trace_new(trace_callback callback, void *userdata);

/* Release `struct trace`, stop tracing processes (PTRACE_DETACH). */
void trace_free(struct trace *trace);

/* Run a traced process. */
int trace_execvp(struct trace *trace, char **argv);

/* Get a signal file descriptor. If readable call `trace_read`. */
int trace_sfd(struct trace *trace);

/* Number of actively traced processes. */
int trace_process_count(struct trace *trace);

/* Main loop, call it when `sfd` is readable. */
void trace_read(struct trace *trace);

/* Set process callback. */
int trace_continue(struct trace_process *process,
		   trace_callback callback, void *userdata);

/* Set registers back in the process. Only makes sense during
 * TRACE_SYSCALL_* callback. */
void trace_setregs(struct trace_process *process, struct trace_sysarg *sysarg);

/* Copy data to and from a process. Data length and addres must be
   word-aligned. */
int copy_from_user(struct trace_process *process, void *dst,
		   unsigned long src, size_t len);
int copy_to_user(struct trace_process *process, unsigned long dst,
		 void *src, size_t len);
