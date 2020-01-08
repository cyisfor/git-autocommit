#include "continuation.h"

typedef void (*runner)(void* arg, struct continuation after);

void hook_run(struct event_base* eventbase, const char* name, const size_t nlen, struct continuation after);
void hooks_init(struct event_base* eventbase);
#define HOOK_RUN(eventbase, name,after) hook_run(eventbase, name,sizeof(name)-1, after)
