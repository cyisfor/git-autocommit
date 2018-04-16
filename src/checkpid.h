#include "continuation.h"
void checkpid_init(void);
void checkpid_after(int pid, struct continuation later);
int checkpid_fork();

void checkpid(int pid, const char* fmt, ...);
