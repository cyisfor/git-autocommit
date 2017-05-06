void hook_run(const char* name, const size_t nlen, uv_async_t* after);
void hooks_init(void);
#define HOOK_RUN(name,after) hook_run(name,sizeof(name)-1, after)
