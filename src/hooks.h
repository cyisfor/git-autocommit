#include "continuation.h"

typedef void (*runner)(void* arg, struct continuation after);

void hook_run(const char* name, const size_t nlen, struct continuation after);
void hooks_init(void);
#define HOOK_RUN(name,after) hook_run(name,sizeof(name)-1, after)
