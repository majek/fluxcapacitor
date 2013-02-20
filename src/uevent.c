#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include "uevent.h"


struct uevent *uevent_new(struct uevent *uevent) {
	if (!uevent)
		uevent = malloc(sizeof(struct uevent));
	memset(uevent, 0, sizeof(struct uevent));
	uevent->used_slots = 0;
	uevent->max_fd = 0;
	FD_ZERO(&uevent->readfds);
	FD_ZERO(&uevent->writefds);
	return uevent;
}

int uevent_select(struct uevent *uevent, struct timeval *timeout) {
	fd_set rfds;
	fd_set wfds;
	memcpy(&rfds, &uevent->readfds, sizeof(fd_set));
	memcpy(&wfds, &uevent->writefds, sizeof(fd_set));
	int r = select(uevent->max_fd + 1, &rfds, &wfds, NULL, timeout);
	if (-1 == r) {
		if (EINTR != errno) {
			perror("select()");
			abort();
		}
	}
	int i;
	for (i=0; i < uevent->max_fd+1; i++) {
		int mask = 0;
		if (FD_ISSET(i, &rfds)) {
			mask |= UEVENT_READ;
		}
		if (FD_ISSET(i, &wfds)) {
			mask |= UEVENT_WRITE;
		}
		if (mask) {
			uevent->fdmap[i].callback(uevent, i, mask,
						  uevent->fdmap[i].userdata);
		}
	}
	return r;
}

int uevent_loop(struct uevent *uevent) {
	int counter = 0;
	while (uevent->used_slots) {
		uevent_select(uevent, NULL);
		counter++;
	}
	return counter;
}


int uevent_yield(struct uevent *uevent, int fd, int mask,
		 uevent_callback_t callback, void *userdata) {
	if (fd >= __FD_SETSIZE) {
		fprintf(stderr, "Can't handle more than %i descriptors.",
			__FD_SETSIZE);
		abort();
	}
	if (!callback) {
		abort();
	}

	if (mask & UEVENT_READ) {
	 	FD_SET(fd, &uevent->readfds);
	}
	if (mask & UEVENT_WRITE) {
		FD_SET(fd, &uevent->writefds);
	}
	if (!uevent->fdmap[fd].callback) {
		uevent->used_slots++;
		if (uevent->max_fd < fd) {
			uevent->max_fd = fd;
		}
	}
	uevent->fdmap[fd].callback = callback;
	uevent->fdmap[fd].userdata = userdata;
	return 1;
}

void uevent_clear(struct uevent *uevent, int fd) {
	FD_CLR(fd, &uevent->readfds);
	FD_CLR(fd, &uevent->writefds);
	uevent->fdmap[fd].callback = NULL;
	uevent->fdmap[fd].userdata = NULL;
	uevent->used_slots--;
}

