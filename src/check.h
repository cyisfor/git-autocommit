#include <event2/event.h>

void check_accept(uv_stream_t* server);
void check_init(struct event_base* base, int sock);
