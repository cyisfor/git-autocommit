#include "eventbase.h"

struct event_base* base = NULL;

void eventbase_init(void) {
	base = event_base_new();
}
