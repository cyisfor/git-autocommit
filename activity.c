#include <uv.h>
#include <stdlib.h> // exit

static uv_timer_t activity;
static void too_idle(uv_timer_t* handle) {
	exit(0);
}

#define DELAY 60*60 * 1000

void activity_poke(void) {
	uv_timer_stop(&activity);
	uv_timer_start(&activity, too_idle, DELAY, 0);
}

void activity_init(void) {
	uv_timer_init(uv_default_loop(), &activity);
	uv_timer_start(&activity, too_idle, DELAY, 0);
}
