#ifndef _UEVENT_H
#define _UEVENT_H

struct uevent;

typedef int (*uevent_callback_t)(struct uevent *uevent, int sd, int mask, void *userdata);

struct uevent {
	int max_slots;
	int curr_slot;
	int used_slots;
	
	struct {
		uevent_callback_t callback;
		void *userdata;
	} fdmap[__FD_SETSIZE];
	
	fd_set readfds;
	fd_set writefds;
	int max_fd;
	
};

enum {
	UEVENT_WRITE	= 1 << 0,
	UEVENT_READ	= 1 << 1
};


struct uevent *uevent_new(struct uevent *uevent);
int uevent_loop(struct uevent *uevent);
int uevent_select(struct uevent *uevent, struct timeval *timeout);

int uevent_yield(struct uevent *uevent, int fd, int mask, uevent_callback_t callback, void *userdata);
void uevent_clear(struct uevent *uevent, int fd);


#endif // _UEVENT_H
