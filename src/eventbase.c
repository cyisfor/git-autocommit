#include "eventbase.h"
#include <event2/event.h> 

struct event_base* eventbase_init(void) {
	struct event_config* cfg = event_config_new();
	event_config_set_flag(cfg,
						  EVENT_BASE_FLAG_NOLOCK |
						  EVENT_BASE_FLAG_STARTUP_IOCP);
	
	return event_base_new_with_config(cfg);
}
