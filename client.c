#define _GNU_SOURCE
#include "ops.h"
#include "repo.h"
#include "net.h"

#include <uv.h>

#include <sys/socket.h> // 
#include <sys/un.h> // 

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

FILE* setuplog(void) {
	if(getenv("ferrets") != NULL) {
		return stdout;
	}
			
	int logloc = open_home();
	move_to(logloc, ".local", "logs", NULL);

	int log = openat(logloc,"autocommit.log", O_WRONLY|O_CREAT|O_APPEND, 0644);
	assert(log >= 0);
	dup2(1,logloc); // logloc isn't in use anymore
	dup2(log,1);
	dup2(log,2);
	close(log);
	log = logloc; // HAX
	return fdopen(log,"wt");
}

int main(int argc, char *argv[])
{
	// first arg = name of file that was saved


	FILE* message = setuplog();

	// now everything written to "message" goes to stdout (emacs)
	// while stdout/err goes to a log

	void bye(const char* fmt, ...) {
		va_list args;
		va_start(args,fmt);
		vfprintf(message, fmt, args);
		va_end(args);
		fputc('\n',message);
		exit(23);
	}

	bool quitting = (NULL != getenv("quit"));
	bool checking = (NULL != getenv("check"));
	enum operations op = quitting ? QUIT : checking ? INFO : ADD;
		
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
		printf("repo relative path %s\n",path);
	}
	
	net_set_addr();

	/* the strategy is... continue trying to connect to addr
		 if ECONNREFUSED, try binding
		 if bound, listen, fork and hand over the socket, then go back to connecting
	*/
	
	uv_pipe_t conn;
	uv_write_t writing;
	uv_pipe_init(uv_default_loop(), &conn, 1);


	int tries = 0;
	uv_timer_t trying;
	uv_timer_init(uv_default_loop(),&trying);

	void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* ret) {
		ret->base = malloc(size);
		ret->len = size;
	}

	void get_info(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
		assert(nread == sizeof(pid_t));
		pid_t pid = *((pid_t*)buf->base);
		fprintf(message, "Server PID: %ld\n",pid);
		uv_read_stop(stream);
	}

	void cleanup(uv_write_t* req, int status) {
		if(op == INFO) {
			uv_read_start((uv_stream_t*)&conn, alloc_cb, get_info);
		} else {
			exit(0);
		}
	}
	
	void on_connect(void) {
		tries = 0;
		uv_timer_stop(&trying);

		uv_buf_t dest;
		if(op == ADD) {
			dest.len = plen+2;
			dest.base = alloca(dest.len);
			*((u16*)dest.base) = dest.len - 2;
			memcpy(dest.base + 2, path, dest.len - 2);
		} else {
			// operations besides ADD are rare, so save a 1 byte op for ADD,
			// add a 2-byte 0-size for "not ADD"
			dest.len = 3;
			dest.base = alloca(3);
			*((u16*)dest.base) = 0;
			dest.base[2] = op;
		}
		uv_write(&writing, (uv_stream_t*) &conn, &dest, 1, cleanup);
	}

	void try_connect(void) {
		// uv_pipe_connect fails on abstract sockets
		// https://github.com/joyent/libuv/issues/1486
		int sock = net_connect();
		if(sock == -1) {
			if(++tries != 3) return;

			if(quitting) exit(0);
			
			// try to bind
			int sock = net_bind();
			if(sock <= 0) return; // already bound, hopefully
			
			// we got it. start the server
			int pid = fork();
			if(pid == 0) {
				setsid();

				// don't bother saving stdout... emacs ignores stdout after process is gone
				// dup2(log,1);
				// make sure the socket is on fd 3
				// we could snprintf(somebuf,0x10,"%d",sock) for the execve, then atoi, but dup2 is cheaper
				if(sock == 3) {
					fcntl(sock,F_SETFL,FD_CLOEXEC | fcntl(sock,F_GETFL));
				} else {
					dup2(sock,3); // note: dup2 clears FD_CLOEXEC
					close(sock);
				}

				// emacs tries to trap you by opening a secret unused pipe
				int i;
				for(i=sock+1;i < sock+10; ++i) {
					close(i);
				}
				close(0);

				/* now
					 0 => nothing
					 1 => log
					 2 => log
					 3 => bound, listening socket
					 4+ => nothing
				*/

				setenv("bound","1",1);
				
				// ...client -> ...server
				size_t len = strlen(argv[0]);
				argv[0][len-6] = 's';
				argv[0][len-5] = 'e';
				argv[0][len-4] = 'r';
				argv[0][len-3] = 'v';
				argv[0][len-2] = 'e';
				argv[0][len-1] = 'r';
				execl(argv[0],argv[0],git_repository_path(repo),NULL);
			}
			fprintf(message,"AC: starting server %d\n",pid);
			close(sock); // XXX: could we finagle this socket into a connected one without closing it?
			try_connect(); // we should be able to connect right away since listen() already called
		} else {
			assert(0==uv_pipe_open(&conn, sock));
			on_connect();
		}
	}
	uv_timer_start(&trying, (void*)try_connect, 0, 200);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	return 0;
}
