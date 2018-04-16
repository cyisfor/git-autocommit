#include "activity.h"
#include "eventbase.h"

#include <stdlib.h> // exit

static struct event* activity;
static void too_idle() {
	exit(0);
}

const struct timeval delay = {
	.tv_sec = 60 * 60 * 1000
}

void activity_poke(void) {
	evtimer_del(activity);
	evtimer_add(activity, &delay);
}

void activity_init(void) {
	activity = evtimer_new(base, too_idle, NULL);
	evtimer_add(activity, &delay);
}
