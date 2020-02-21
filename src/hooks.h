#include "continuation.h"
#include "mystring.h"

typedef void (*runner)(void* arg, struct continuation after);

void hook_run(struct event_base* eventbase, const string name, struct continuation after);
void hooks_init(struct event_base* eventbase, const string project);
#define HOOK_RUN(eventbase, name,after) hook_run(eventbase, LITSTR(name), after)
