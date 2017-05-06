void hook_run(int wait, const char* name, const size_t nlen);
void hooks_init(void);
#define HOOK_RUN(a,b) hook_run(b,a,sizeof(a)-1)
