#include "checkpid.h"
#include "myassert.h"

#include <signal.h> // sigaction
#include <stdio.h>
#include <stdarg.h> // va_*
#include <sys/wait.h> // waitpid

struct after {
	int pid;
	uv_async_t* async;
	struct after* prev;
	struct after* next;
};

struct after* afters;

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
		} else {
			erra("whu? %d",status);
		}

		struct after* cur = afters;
		while(cur) {
			if(cur->pid == pid) {
				// not safe to free async until it is received
				uv_async_send(cur->async);
				free(cur);
			}
		}
	}
}

void checkpid_after(int pid, uv_async_t* async) {
	struct after* a = malloc(sizeof(struct after));
	a->pid = pid;
	a->async = async;
	a->next = afters;
	a->prev = NULL;
	afters->prev = a;
	afters = a;
}

void checkpid_init(void) {
	struct sigaction sa = {
		.sa_handler = onchld,
		.sa_flags = SA_RESTART | SA_NOCLDSTOP
	};
	assert0(sigaction(SIGCHLD,&sa,NULL));
}

void checkpid(int pid, const char* fmt, ...) {
	printf("launch %d ",pid);
	va_list arg;
	va_start(arg, fmt);
	vfprintf(stdout, fmt, arg);
	va_end(arg);
	fputc('\n',stdout);
	// then just let the handler catch it...
	// we don't want to allocate memory for messages about every pending process?
}
