#include "continuation.h"
void continuation_run(struct continuation c) {
	event_base_once(base, -1, EV_TIMEOUT, c.func, c.arg, NULL);
}
