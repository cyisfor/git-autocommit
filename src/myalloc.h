#define malloc myalloc
#define calloc mycalloc
#define free myfree

void* myalloc(size_t);
void* mycalloc(size_t,size_t);
void myfree(void*);
void* mycalloc(size_t,size_t);
