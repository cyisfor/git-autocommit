#include "note.h" // error(...)
#include <stdlib.h> // abort
#include <stdint.h> // intptr_t


#define ensure0(test) { intptr_t res = ((intptr_t)test);	\
		if(res != 0) {																				\
			ERROR(#test " %d not zero",res);										\
			abort();																						\
		}																											\
	}

#define ensure_eq(less,more) { intptr_t res1 = ((intptr_t)less);	\
		intptr_t res2 = ((intptr_t)more);															\
		if(res1 != res2) {																						\
			ERROR(#less " %d != " #more " %d",res1,res2);								\
			abort();																										\
		}																															\
	}

#define ensure_ne(less,more) { intptr_t res1 = ((intptr_t)less);	\
		intptr_t res2 = ((intptr_t)more);															\
		if(res1 == res2) {																						\
			ERROR(#less " %d == " #more " %d",res1,res2);								\
			abort();																										\
		}																															\
	}


#define ensure_gt(less,more) { intptr_t res1 = ((intptr_t)less);	\
		intptr_t res2 = ((intptr_t)more);															\
		if(res1 <= res2) {																						\
			ERROR(#less " %d <= " #more " %d",res1,res2);								\
			abort();																										\
		}																															\
	}

#define ensure_ge(less,more) { intptr_t res1 = ((intptr_t)less);	\
		intptr_t res2 = ((intptr_t)more);															\
		if(res1 < res2) {																							\
			ERROR(#less " %d < " #more " %d",res1,res2);								\
			abort();																										\
		}																															\
	}
