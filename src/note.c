#include "note.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h> // abort, getenv
#include <stdarg.h> // va_*
#include <errno.h>

struct note_options note_options = {};

static bool jumping = false;
static jmp_buf jumper;

static void note(const char* how, int hlen,
								 const char* file, int flen,
								 int line, const char* fmt, va_list arg) {
	if(note_options.show_method) {
		fwrite(how, hlen, 1, stderr);
		fputc(' ',stderr);
	}
	if(note_options.show_location) {
		fwrite(file, flen, 1, stderr);
		fprintf(stderr,":%d ",line);
	}
	vfprintf(stderr,fmt,arg);
	va_end(arg);
	fputc('\n',stderr);
}

#define BODY(name) \
	va_list arg;																													\
	va_start(arg, fmt);																										\
	note(name,sizeof(name)-1,file,flen,line,fmt,arg);											\

#define HEAD(ident) void ident ## f(const char* file, int flen, int line, const char* fmt, ...)

#define DEFINE(ident,name) HEAD(ident) { \
		BODY(name)																																\
}

DEFINE(spam,"DEBUG");
DEFINE(info,"INFO");
DEFINE(warn,"WARN");

HEAD(error) {
	BODY("ERROR");
	if(errno) {
		perror("errno");
	}
	if(getenv("error_nonfatal")) return;
	if(jumping) {
		longjmp(jumper, 1);
	}
	abort();
}

bool note_catch(void) {
	jumping = true;
	return 0 != setjmp(jumper);
}

void note_init(void) {
	note_options.show_method = (NULL==getenv("note_hidemethod"));
	note_options.show_location = (NULL!=getenv("note_location"));
}
