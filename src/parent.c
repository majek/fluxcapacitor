#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "list.h"
#include "types.h"
#include "trace.h"
#include "fluxcapacitor.h"

extern struct options options;


struct parent *parent_new() {
	struct parent *parent = calloc(1, sizeof(struct parent));

	INIT_LIST_HEAD(&parent->list_of_childs);
	INIT_LIST_HEAD(&parent->list_of_blocked);

	return parent;
}

void parent_run_one(struct parent *parent, struct trace *trace,
		    char **child_argv) {
	int pid = trace_execvp(trace, child_argv);
	char *flat_argv = argv_join(child_argv, " ");
	SHOUT("[+] %i running: %s", pid, flat_argv);
	free(flat_argv);
}

struct child *parent_min_timeout_child(struct parent *parent) {
	struct child *min_child = NULL;

	/* All childs must be blocking */
	if (parent->blocked_count != parent->child_count)
		FATAL("");

	struct list_head *pos;
	list_for_each(pos, &parent->list_of_childs) {
		struct child *child = hlist_entry(pos, struct child, in_childs);
		if (child->blocked_until != TIMEOUT_UNKNOWN) {
			if (!min_child ||
			    min_child->blocked_until > child->blocked_until) {
				min_child = child;
			}
		}
	}
	if (!min_child ||
	    min_child->blocked_until <= 0 ||
	    min_child->blocked_until >= TIMEOUT_FOREVER) {
		return NULL;
	}
	return min_child;
}

struct child *parent_woken_child(struct parent *parent) {
	struct list_head *pos;
	list_for_each(pos, &parent->list_of_childs) {
		struct child *child = hlist_entry(pos, struct child, in_childs);
		char fname[64];
		snprintf(fname, sizeof(fname), "/proc/%i/stat", child->pid);
		char buf[1024];
		if (read_file(fname, buf, sizeof(buf)) < 16)
			FATAL("%s is too small", fname);
		// pray for a program name without parenthesis
		char *p = strchr(buf, ')');
		if (p[0] != ')')
			FATAL("");
		if (p[1] != ' ')
			FATAL("");
		if (p[2] != 'S')
			return child;
	}
	return NULL;
}

void parent_kill_all(struct parent *parent, int signo) {
	struct list_head *pos;
	list_for_each(pos, &parent->list_of_childs) {
		struct child *child = hlist_entry(pos, struct child, in_childs);
		kill(child->pid, signo);
	}
}

void child_kill(struct child *child, int signo) {
	kill(child->pid, signo);
}

struct child *child_new(struct parent *parent, struct trace_process *process, int pid) {
	struct child *child = calloc(1, sizeof(struct child));
	child->blocked_until = TIMEOUT_UNKNOWN;
	child->pid = pid;
	child->process = process;
	child->parent = parent;
	list_add(&child->in_childs, &parent->list_of_childs);
	parent->child_count += 1;
	return child;
}

void child_del(struct child *child) {
	if (child->blocked)
		child_mark_unblocked(child);
	list_del(&child->in_childs);
	child->pid = 0;
	child->parent->child_count -= 1;
	free(child);
}


void child_mark_blocked(struct child *child) {
	if (child->blocked)
		FATAL("");

	child->blocked = 1;
	child->blocked_until = TIMEOUT_UNKNOWN;
	list_add(&child->in_blocked, &child->parent->list_of_blocked);
	child->parent->blocked_count += 1;
}

void child_mark_unblocked(struct child *child) {
	if (!child->blocked)
		FATAL("");

	child->blocked = 0;
	list_del(&child->in_blocked);
	child->parent->blocked_count -= 1;
}
