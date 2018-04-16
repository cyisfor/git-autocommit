#include "ensure.h"

#include "checkpid.h"
#include "myassert.h"

#include <sys/signalfd.h>
#include <signal.h>

#include <stdio.h>
#include <stdarg.h> // va_*
#include <sys/wait.h> // waitpid
#include <unistd.h> // fork

struct after {
	int pid;
	struct continuation later;
	struct after* prev;
	struct after* next;
};

struct after* afters = NULL;

static
void read_from_signalfd(evutil_socket_t sfd, short events, void * arg) {
	/* ignore the data read.
		 ssi_status only gets the first process, not all of them */
	static char buf[0x100];
	for(;;) {
		ssize_t amt = read(sfd, buf, 0x100);
		if(amt == 0) break;
		if(amt < 0) {
			if(errno != EAGAIN) {
				perror("read signalfd");
				abort();
			}
			break;
		}
	}
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
		if(pid < 0) {
			ensure_eq(errno,ECHILD);
			break;
		}
		
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
				if(cur == afters) {
					afters = cur->next;
				}
				if(cur->next) {
					cur->next->prev = cur->prev;
				}
				if(cur->prev) {
					cur->prev->next = cur->next;
				}
				continuation_run(cur->later);
				free(cur);
				break;
			}
			cur = cur->next;
		}
	}
}				

void checkpid_after(int pid, struct continuation later) {
	struct after* a = malloc(sizeof(struct after));
	a->pid = pid;
	a->later = later;
	a->next = afters;
	a->prev = NULL;
	if(afters) {
		afters->prev = a;
	}
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
	struct bufferevent* b = 
	uv_read_start((uv_stream_t*)&sfd, alloc_cb, get_reply);
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
