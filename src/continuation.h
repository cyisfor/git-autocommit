#ifndef CONTINUATION_H
#define CONTINUATION_H

#include <event2/event.h>


struct continuation {
	struct event_base* eventbase;
	void (*func)(void*);
	void* arg;
};

void continuation_run(const struct continuation c);

#endif /* CONTINUATION_H */
