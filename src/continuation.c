#include "continuation.h"
#include "eventbase.h"
#include <event2/event.h>
#include <stdlib.h> // free

static
void rundatting(evutil_socket_t nothing, short events, void* arg) {
	struct continuation c = *((struct continuation*) arg);
	free(arg);
	c.func(c.arg);
}

void continuation_run(struct continuation c) {
	if(c.func == NULL) return;
	struct continuation* cc = malloc(sizeof(struct continuation));
	cc->func = c.func;
	cc->arg = c.arg;
	event_base_once(base, -1, EV_TIMEOUT, rundatting, cc, NULL);
}
