#include "myassert.h"

#include <signal.h> // sigaction
#include <stdio.h>
#include <stdarg.h> // va_*



static void onchld(int sig) {
	int pid;
	int status;
	void erra(char* fmt, ...) {
		fprintf(stderr,"pid %d ",pid);
		va_list arg;
		va_start(arg, fmt);
		vfprintf(stdout, fmt, arg);
		va_end(arg);
		fputc('\n',stderr);
	}
	while(-1 != (pid = waitpid(-1, &status, WNOHANG))) {
		if(WIFSIGNALED(status)) {
			erra("died with signal %d",WTERMSIG(status));
	} else if(WIFEXITED(status)) {
		int res = WEXITSTATUS(status);
		if(res != 0) {
			erra("exited with %d",res);
		}
	}
}

void checkpid_init(void) {
	struct sigaction sa = {
		.sa_handler = onchld,
		.sa_flags = SA_RESTART | SA_NOCLDSTOP
	};
	assert0(sigaction(SIGCHLD,&sa,NULL));
}

void checkpid(int pid, char* fmt, ...) {
	printf("launch %d ",pid);
	va_list arg;
	va_start(arg, fmt);
	vfprintf(stdout, fmt, arg);
	va_end(arg);
	fputc('\n',stdout);
}
