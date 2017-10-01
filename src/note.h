#ifndef _NOTE_H_
#define _NOTE_H_


void infof(const char* file, int flen, int line, const char* fmt, ...);
void warnf(const char* file, int flen, int line, const char* fmt, ...);
void errorf(const char* file, int flen, int line, const char* fmt, ...);
void spamf(const char* file, int flen, int line, const char* fmt, ...);

#define INFO(args...) infof(__FILE__,sizeof(__FILE__)-1,__LINE__, args)
#define WARN(args...) warnf(__FILE__,sizeof(__FILE__)-1,__LINE__, args)
#define ERROR(args...) errorf(__FILE__,sizeof(__FILE__)-1,__LINE__, args)

#ifndef NDEBUG
#define SPAM(args...) spamf(__FILE__,sizeof(__FILE__)-1,__LINE__, args)
#else
#define SPAM(...)
#endif

#include <stdbool.h>

bool note_catch(void);

struct note_options {
	bool show_method;
	bool show_location;
};

extern struct note_options note_options;

void note_init(void);


#endif /* _NOTE_H_ */
