#include "continuation.h"
void checkpid_init(struct event_base* eventbase);
void checkpid_after(int pid, struct continuation later);
int checkpid_fork();

void checkpid(int pid, const char* fmt, ...);
