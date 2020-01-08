#include "activity.h"

#include <stdlib.h> // exit

static struct event* activity;
static void too_idle() {
	exit(0);
}

static
const struct timeval activity_delay = {
	.tv_sec = 60 * 5
};

void activity_poke(void) {
	evtimer_del(activity);
	evtimer_add(activity, &activity_delay);
}

void activity_init(struct event_base* eventbase) {
	activity = evtimer_new(eventbase, (void*)too_idle, NULL);
	evtimer_add(activity, &activity_delay);
}
