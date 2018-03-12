#define _GNU_SOURCE
#include "ops.h"
#include "repo.h"
#include "net.h"
#include "check.h"

#include <uv.h>

#include <sys/socket.h> // 
#include <sys/un.h> //
#include <sys/resource.h> // setrlimit


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
bool quitting = false;
bool checking = false;

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
void onsig(int signal) {
	kill(server_pid,signal);
	exit(signal);
}


char buf[0x1000];
int sock = -1;

void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* ret) {
	ret->base = buf;
	ret->len = 0x1000;
}


void (*csucks)(void) = NULL;

void restart_when_closed(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
	if(nread > 0) return;
	if(nread != UV_ECONNRESET && nread != UV_EOF) {
		printf("um %s\n",uv_strerror(nread));
		abort();
	}
	uv_close((uv_handle_t*)stream, (void*)csucks);
}

int tries = 0;
uv_timer_t trying;

void kill_remote(uv_stream_t* stream, ssize_t err) {
	printf("Killing remote... %s %ld %d\n",uv_strerror(err), err, net_pid(sock));
	uv_read_stop(stream);
	uv_close((uv_handle_t*)stream, (void*)csucks);
	kill(net_pid(sock),SIGTERM);
}

enum operations op;

void get_reply(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
	uv_timer_stop(&trying);
	if(op == INFO) {
		if(nread != sizeof(struct info_message)) {
			kill_remote(stream, nread);
			return;
		}
		struct info_message* im = (struct info_message*)buf->base;
			
		printf("Server Info: pid %d (%d)\n"
					 "Lines %lu Words %lu Characters %lu\n",
					 im->pid, net_pid(sock),
					 im->lines, im->words, im->characters);
		if(im->next_commit) {
			printf(
				"Next commit: %lu (in %ld)\n",
				im->next_commit, im->next_commit - time(NULL));
		} else {
			puts("No commit scheduled");
		}
			
		uv_read_stop(stream);
	} else {
		if(nread == 0) return;
		if(nread != 1) {
			kill_remote(stream, nread);
			return;
		}
		char res = *buf->base;
		switch(res) {
		case 0:
			// success
			uv_read_stop(stream);
			break;
		default:
			printf("Got weirdness %d\n",res);
			abort();
		};
	}
}


uv_pipe_t conn;
uv_write_t writing;

void retry(uv_timer_t* timer) {
	kill_remote((uv_stream_t*)&conn, 0);
	csucks();
} 

void await_reply(uv_write_t* req, int status) {
	uv_read_start((uv_stream_t*)&conn, alloc_cb, get_reply);
	uv_timer_start(&trying, (void*)retry, 1000000, 0);
}

void on_connect(void) {
	tries = 0;
	uv_timer_stop(&trying);

	const uv_buf_t dest = { (char*)&op, 1 };
	uv_write(&writing, (uv_stream_t*) &conn, &dest, 1, await_reply);
}



jmp_buf start_watcher;	
	
void try_connect() {
	// uv_pipe_connect fails on abstract sockets
	// https://github.com/joyent/libuv/issues/1486
	sock = net_connect();
	if(sock == -1) {
		if(++tries > 3) {
			if(debugging_fork) {
				sleep(3);
				return try_connect();
			} else {
				printf("Couldn't spawn server %d\n",tries);
				abort();
			}
		}

		if(quitting) exit(0);
		if(checking && NULL == getenv("start")) {
			printf("Server not running.\n");
			exit(1);
		}
			
		// try to bind
		sock = net_bind();
		if(sock <= 0) return; // already bound, hopefully
		printf("Got socket %d\n",sock);

		// we got it. start the server
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
				uv_timer_stop(&trying);
				// call check_init directly, instead of wasting time with execve
				check_init(sock);

				// already started out uv_run in the parent process
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
		close(sock); // XXX: could we finagle this socket into a connected one without closing it?
		usleep(100000); // XXX: mysterious race condition... activity on the client's sockets created AFTER the server is forked, are reported to the server process?
		try_connect(); // we should be able to connect right away since listen() already called
	} else {
		int res = uv_pipe_open((uv_pipe_t*)&conn, sock);
		if(res == 0) {
			on_connect();
		} else {
			printf("um %s\n",uv_strerror(res));
			close(sock);
		}
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
		// with no uv_loop hanging out there.
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

	void bye(const char* fmt, ...) {
		va_list args;
		va_start(args,fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
		fputc('\n',stderr);
		exit(23);
	}

	quitting = (NULL != getenv("quit"));
	if(quitting) {
		op = QUIT;
	} else {
		checking = (NULL != getenv("check"));
		if(checking) {
			op = INFO;
		} else if(NULL != getenv("force")) {
			op = FORCE;
		} else {
			op = ADD;
		}
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
	
	writing.data = &conn;
	uv_pipe_init(uv_default_loop(), &conn, 1);

	uv_timer_init(uv_default_loop(),&trying);
	trying.data = &conn;

	debugging_fork = getenv("debugfork") != NULL;

	csucks = try_connect;
	uv_timer_start(&trying, (void*)try_connect, 0, 200);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	return 0;
}
