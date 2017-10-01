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

struct after* afters = NULL;

static void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* ret) {
	ret->len = (((size / sizeof(struct signalfd_siginfo))+1)*sizeof(struct signalfd_siginfo));
	ret->base = realloc(ret->base,ret->len);
}

static void get_reply(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
	ensure_ge(nread,0);
	/* ignore the data read. ssi_status only gets the first process, not all of them */
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
	for(;;) {
		pid = waitpid(-1, &status, WNOHANG);
		if(pid == 0) break;
		ensure_gt(pid,0);
		
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
				if(cur->next) {
					cur->next->prev = cur->prev;
				}
				if(cur->prev) {
					cur->prev->next = cur->next;
				}
				free(cur);
			}
			cur = cur->next;
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

static sigset_t blocked;
static uv_pipe_t sfd;

// eh... only useful if you're about to exec.
int checkpid_fork() {
	int pid = fork();
	if(pid == 0) {
		sigprocmask(SIG_UNBLOCK,&blocked, NULL);
	}
	return pid;
}

void checkpid_init(void) {
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGCHLD);
	ensure0(sigprocmask(SIG_BLOCK, &blocked, NULL));

	int s = signalfd(-1, &blocked, 0);
	ensure_ge(s,0);
	ensure0(uv_pipe_init(uv_default_loop(), &sfd, 0));
	ensure0(uv_pipe_open(&sfd, s));
	uv_read_start((uv_stream_t*)&sfd, alloc_cb, get_reply);
}
