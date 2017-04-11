#define _GNU_SOURCE
#include "repo.h"

#include <uv.h>

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

int main(int argc, char *argv[])
{
	// first arg = name of file that was saved

	int logloc = open_home();
	move_to(logloc, ".local", "logs", NULL);
	
	int log = openat(logloc,"autocommit.log", O_WRONLY|O_CREAT|O_APPEND, 0644);
	assert(log >= 0);
	dup2(1,logloc); // logloc isn't in use anymore
	dup2(log,1);
	dup2(log,2);
	close(log);
	log = logloc; // HAX
	FILE* message = fdopen(log,"wt");

	// now everything written to "message" goes to stdout (emacs)
	// while stdout/err goes to a log

	void bye(const char* fmt, ...) {
		va_list args;
		va_start(args,fmt);
		vfprintf(message, fmt, args);
		va_end(args);
		fputc('\n',message);
	}

	const char* path = getenv("file");
	if(path == NULL) {
		bye("no file provided");
	}

	/* if we start out in a subdirectory of a repository, we don't want to run a
		 second server instance of the same repository. Just chdir to the top level.
	*/

	// the name of the socket is \0 plus the git top directory
	char name[PATH_MAX+2] = "\0";

	if(0 != repo_init()) {
		bye("couldn't find a git repository");
	}
	// repo_init chdirs to the git top directory
	realpath(".",name+1);
	//printf("Found git dir '%s'\n",name+1);
	
	uv_pipe_t conn;
	uv_write_t writing;
	uv_pipe_init(uv_default_loop(), &conn, 1);

	void cleanup(uv_write_t* req, int status) {
		exit(0);
	}

	bool quitting = (NULL != getenv("quit"));

	int tries = 0;
	uv_timer_t trying;
	uv_timer_init(uv_default_loop(),&trying);

	void on_connect(uv_connect_t* req, int status) {
		if(status < 0) {
			if(quitting) exit(0);
			if(++tries != 3) return;
			// start the server
			int pid = fork();
			if(pid == 0) {
				setsid();
				// emacs tries to trap you by opening a secret unused pipe
				int i;
				for(i=log+1;i < log+3; ++i) {
					close(i);
				}
				// don't bother saving log... emacs ignores stdout after process is gone
				// dup2(log,1);
				
				// ...client -> ...server
				size_t len = strlen(argv[0]);
				argv[0][len-6] = 's';
				argv[0][len-5] = 'e';
				argv[0][len-4] = 'r';
				argv[0][len-3] = 'v';
				argv[0][len-2] = 'e';
				argv[0][len-1] = 'r';
				name[0] = '@'; // IPC is hard...
				execl(argv[0],argv[0],name,NULL);
			}
			fprintf(message,"AC: starting server %d\n",pid);
			return;
		}
		tries = 0;
		uv_timer_stop(&trying);

		uv_buf_t dest;
		if(quitting) {
			dest.len = 3;
			dest.base = alloca(3);
			*((u16*)dest.base) = 0;
			dest.base[2] = 0;
		} else {
			dest.len = strlen(path)+2;
			dest.base = alloca(dest.len);
			*((u16*)dest.base) = dest.len - 2;
			memcpy(dest.base + 2, path, dest.len - 2);
		}
		uv_write(&writing, (uv_stream_t*) &conn, &dest, 1, cleanup);
	}

	uv_connect_t derp;
	void try_connect() {
		// the name of the socket is \0 plus the git top directory
		uv_pipe_connect(&derp, &conn, name, on_connect);
	}
	uv_timer_start(&trying, try_connect, 0, 200);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	return 0;
}
