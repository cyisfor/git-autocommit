#define _GNU_SOURCE
#include "eventbase.h"
#include "ops.h"
#include "repo.h"
#include "net.h"
#include "check.h"

#include <sys/socket.h> //
#include <sys/un.h> //
#include <sys/resource.h> // setrlimit

#include <limits.h> // PATH_MAX


#include <setjmp.h>
#include <libgen.h> // dirname
#include <stdlib.h> // exit
#include <assert.h>
#include <string.h> // strlen
#include <unistd.h> // getuid, close, dup2, fork
#include <stdbool.h>
#include <sys/wait.h> // waitpid
#include <stdarg.h> // va_*
#include <fcntl.h> // open* O_PATH
#include <pwd.h> // getpw*
#include <sys/stat.h> // mkdir
#include <error.h> //
#include <sys/prctl.h>
#include <stdio.h>

bool debugging_fork = false;

int open_home(void) {
	const char* path = getenv("HOME");
	if(path == NULL) {
		path = getpwuid(getuid())->pw_dir;
		assert(path != NULL);
	}
	// O_PATH works even if the directory has only the execute bit set.
	return open(path,O_PATH|O_DIRECTORY);
}

void move_to(int loc, ...) {
	va_list a;
	va_start(a,loc);
	int cur = loc;
	for(;;) {
		const char* name = va_arg(a,const char*);
		if(name == NULL) break;
		mkdirat(cur,name,0755);
		int new = openat(cur,name,O_PATH);
		assert(new >= 0);
		close(cur);
		cur = new;
	}
	dup2(cur,loc);
}

typedef uint16_t u16;

static
void setuplog(void) {
	if(NULL != getenv("ferrets")) return;
	int logloc = open_home();
	move_to(logloc, ".local", "logs", NULL);

	int log = openat(logloc,"autocommit.log", O_WRONLY|O_CREAT|O_APPEND, 0644);
	assert(log >= 0);
	close(logloc);
	dup2(log,1);
	dup2(log,2);
	close(0);
	close(log);
}

int server_pid = -1;
static
void onsig(int signal) {
	kill(server_pid,signal);
	exit(signal);
}


static
int sock = -1;

static
struct bufferevent* conn = NULL;

static
struct event* trying = NULL;

static const struct timeval trying_timeout = {
	.tv_sec = 0,
	.tv_usec = 5000000
};

static
int tries = 0;

static
void kill_remote(struct bufferevent* conn) {
	printf("Killing remote... %d\n", net_pid(sock));
	reconnect = false;
	kill(net_pid(sock),SIGTERM);
	bufferevent_free(conn);
}

static
void on_events(struct bufferevent *conn, short events, void *ptr) {
	if(events & BEV_EVENT_CONNECTED) {
		puts("connect okay.");
	} else if(events & (BEV_EVENT_TIMEOUT)) {
		puts("timed out... killing server.");
		kill_remote(conn);
	} else if(events & (BEV_EVENT_ERROR|BEV_EVENT_EOF)) {
		puts("closing...");
		if(reconnect) {
			evtimer_add(trying, &trying_timeout);
		} else {
			event_base_loopexit(base, NULL);
		}
	}
}

static
enum operations op;

static
void get_reply(struct bufferevent *conn, void *ctx) {
	struct evbuffer* input = bufferevent_get_input(conn);
	struct info_message im;
	size_t left = evbuffer_get_length(input);		
	if(op == INFO) {
		if(left != sizeof(struct info_message)) {
			kill_remote(conn);
			return;
		}
		int n = evbuffer_remove(input, &im, sizeof(im));
		assert(n == sizeof(im));
		evtimer_del(trying);

		printf("Server Info: pid %d (%d)\n"
					 "Lines %lu Words %lu Characters %lu\n",
					 im.pid, net_pid(sock),
					 im.lines, im.words, im.characters);
		if(im.next_commit) {
			printf(
				"Next commit: %lu (in %ld)\n",
				im.next_commit, im.next_commit - time(NULL));
		} else {
			puts("No commit scheduled");
		}

		bufferevent_free(conn);
		return;
	}
	// all other commands just send a 1 byte pong
	if(left == 0) return;
	if(left != 1) {
		kill_remote(conn);
		return;
	}
	char res;
	int n = evbuffer_remove(input, &res, 1);
	assert(n == 1);
	switch(res) {
	case 0:
		// success
		bufferevent_free(conn);
		break;
	default:
		printf("Got weirdness %d\n",res);
		abort();
	};
}

jmp_buf start_watcher;

void spawn_server(void) {
	if(++tries > 3) {
		if(debugging_fork) {
			sleep(3);
			return try_connect();
		} else {
			printf("Couldn't spawn server %d\n",tries);
			abort();
		}
	}

	if(op == QUIT || op == FORCE) exit(0);
	if(op == INFO && NULL == getenv("start")) {
		printf("Server not running.\n");
		exit(1);
	}
	// try to bind
	sock = net_bind();
	if(sock <= 0) return; // already bound, server is go

	printf("Got bound socket %d. Starting server...\n",sock);

	/*
		1) fork(), child is the server. parent...
		2) fork(), child is the client. parent is the watcher.
		*/
	
	// do not unblock signals, since we're also duping our signalfd.
	int server_pid = fork();
	if(server_pid == 0) {
		setsid();

/*				dup2(sock,3);
					sock = 3; */

		// emacs tries to trap you by opening a secret unused pipe
		int i;
		for(i=sock+1;i < sock+10; ++i) {
			close(i);
		}

		setuplog();

		int watcher = fork();
		if(watcher == 0) {
			if(debugging_fork) {
				int targetpid = getpid();
				int gdb = fork();
				if(gdb == 0) {
					char buf[100];
					snprintf(buf,100,"%d",targetpid);
					execlp("xfce4-terminal","xfce4-terminal","-x","gdb","-p",buf,NULL);
					abort();
				}
				waitpid(gdb,NULL,0);
				sleep(3);
			}
			evtimer_del(trying);
			// call check_init directly, instead of wasting time with execve
			check_init(sock);

			// already started out dispatch in the parent process
			// now we're the server, so just go back to the loop
			// forget about longjmp that's just for the watcher process
			return;
		}
		close(sock);
		longjmp(start_watcher, watcher);
		abort();
	}
	assert(server_pid > 0);

	printf("starting server %d\n",server_pid);
	net_forkhack(server_pid);
	//usleep(100000); // XXX: mysterious race condition... activity on the client's sockets created AFTER the server is forked, are reported to the server process?
	return reconnect(sock); // we should be able to connect right away since listen() already called
}

static
void try_connect() {
	return reconnect(net_connect());
}

static
void wrote_response(struct bufferevent* conn, void* arg) {
	bufferevent_setcb(conn, get_reply, NULL, on_event, NULL);
	bufferevent_setwatermark(conn, EV_READ, 1, 1 + sizeof(struct info_message));
	bufferevent_disable(EV_WRITE);
	bufferevent_enable(EV_READ);
}

static
void reconnect(int sock) {
	if(sock == -1) {
		return spawn_server();
	}

	if(conn) {
		puts("Stopping bad client");
		bufferevent_free(conn);
	}
	puts("Starting client");
	conn = bufferevent_socket_new(base, sock, BEV_OPT_CLOSE_ON_FREE);
	
	if(conn) {
		const struct timeval timeout = {
			.tv_sec = 0,
			.tv_usec = 500000
		};
		bufferevent_set_timeouts(conn, &timeout, &timeout);
		bufferevent_setcb(conn, get_reply, wrote_response, on_event, NULL);
		bufferevent_setwatermark(conn, EV_WRITE, 1, 1);
		bufferevent_write(conn, &op, 1);
		bufferevent_enable(EV_WRITE);
	} else {
		perror("um");
		int pid = net_pid(sock);
		if(pid > 0) {
			kill(pid,SIGTERM);
		}
		close(sock);
		// try again later, I guess...
		evtimer_add(trying, &trying_timeout);
	}
}

int main(int argc, char *argv[])
{
	// env "file" = name of file that was saved

	struct rlimit rlim = {
		2*1024*1024*1024L,
		2*1024*1024*1024L
	};
	setrlimit(RLIMIT_AS, &rlim);

	if(0 != (server_pid = setjmp(start_watcher))) {
		// keep our retarded watcher nice and retardedly simple,
		// with no libevent loop hanging out there.
		assert(server_pid>0);
		printf("watching PID %d\n",server_pid);
		signal(SIGTERM,onsig);
		signal(SIGINT,onsig);
		signal(SIGQUIT,onsig);
		int res;
		waitpid(server_pid,&res,0);
		if(WIFEXITED(res)) {
			if(0 != WEXITSTATUS(res)) {
				printf("watcher: %d server exited with %d\n",server_pid,WEXITSTATUS(res));
			}
		} else if(WIFSIGNALED(res)) {
			printf("watcher: %d server died with signal %d\n",server_pid,WTERMSIG(res));
		}
		exit(0);
	}

	// now everything written to "message" goes to stdout (emacs)
	// while stdout/err goes to a log

	eventbase_init();

	void bye(const char* fmt, ...) {
		va_list args;
		va_start(args,fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
		fputc('\n',stderr);
		exit(23);
	}

	if(NULL != getenv("quit")) {
		op = QUIT;
	} else if(NULL != getenv("check")) {
		op = INFO;
	} else if(NULL != getenv("force")) {
		op = FORCE;
	} else {
		op = ADD;
	}

	char* path;
	char bigpath[PATH_MAX];
	size_t plen;
	if(op == ADD) {
		path = getenv("file");
		if(path == NULL) {
			bye("no file provided");
		}
		assert(NULL!=realpath(path,bigpath));
		path = bigpath;
		plen = strlen(path);
	} else {
		path = ".";
		plen = 1;
	}

	if(0 != repo_discover_init(path,plen)) {
		bye("couldn't find a git repository");
	}

	if(op == ADD) {
		// hissy fit......
		printf("path %s\n",path);
		plen = repo_relative(&path, plen);
		// printf("repo relative path %s\n",path);
		repo_add(path);
	}

	net_set_addr();

	/* the strategy is... continue trying to connect to addr
		 if ECONNREFUSED, try binding
		 if bound, listen, fork and hand over the socket, then go back to connecting
	*/

	trying = evtimer_new(base, &try_connect);
	evtimer_add(trying, NULL);

	debugging_fork = getenv("debugfork") != NULL;

	event_base_dispatch(base);
	return 0;
}
