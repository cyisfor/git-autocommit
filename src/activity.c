#include "activity.h"
#include "eventbase.h"

#include <stdlib.h> // exit

static struct event* activity;
static void too_idle() {
	exit(0);
}

static
const struct timeval activity_delay = {
	.tv_sec = 60 * 60
};

void activity_poke(void) {
	evtimer_del(activity);
	evtimer_add(activity, &activity_delay);
}

void activity_init(void) {
	activity = evtimer_new(base, (void*)too_idle, NULL);
	evtimer_add(activity, &activity_delay);
}
