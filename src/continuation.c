#include "continuation.h"
#include "eventbase.h"
#include <event2/event.h>

void continuation_run(struct continuation c) {
	if(c.func == NULL) return;
	event_base_once(base, -1, EV_TIMEOUT, c.func, c.arg, NULL);
}
