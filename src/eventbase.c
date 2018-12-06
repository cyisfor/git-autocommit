#include "eventbase.h"

struct event_base* base = NULL;

void eventbase_init(void) {
	struct event_config* cfg = event_config_new();
	event_base_config_set_flag(cfg,
														 EVENT_BASE_FLAG_NOLOCK |
														 EVENT_BASE_FLAG_STARTUP_IOCP);

	base = event_base_new_with_config(cfg);
}
