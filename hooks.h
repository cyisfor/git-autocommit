void hook_run(const char* name, const size_t nlen);
void hooks_init(void);
#define HOOK_RUN(a) hook_run(a,sizeof(a)-1)
