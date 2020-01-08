#ifndef CONTINUATION_H
#define CONTINUATION_H

#include <event2/event.h>


struct continuation {
	void (*func)(void*);
	void* arg;
};

void continuation_run(struct event_base*, const struct continuation c);

#endif /* CONTINUATION_H */
