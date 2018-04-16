#include "eventbase.h"

void eventbase_init(void) {
	base = event_base_new();
}
