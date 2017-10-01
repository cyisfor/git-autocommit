#include <uv.h>
void checkpid_init(void);
void checkpid_after(int pid, uv_async_t* async);
int checkpid_fork();

void checkpid(int pid, const char* fmt, ...);
