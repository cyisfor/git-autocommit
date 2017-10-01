#ifdef NDEBUG
#define assert(v) v
#define assert_ne(a,v) v
// etc
#else
#include <stdlib.h> // abort
#define assert(v) if(!(v)) abort();
#define assert_ne(a,v) if((a) != (v)) abort();
#define assert_gt(a,v) if((a) >= (v)) abort();
#define assert0(v) if(0 != (v)) abort();
#endif
