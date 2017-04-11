#include <uv.h>
#include <stdlib.h> // exit
#include <stdint.h>
#include <assert.h>
#include <string.h> // strlen
#include <unistd.h> // fork, execv
#include <stdbool.h> 
#include <sys/wait.h> // waitpid


typedef uint16_t u16;

int main(int argc, char *argv[])
{
	// first arg = name of file that was saved
	assert(argc == 2);

	/* if we start out in a subdirectory of a repository, we don't want to run a
		 second server instance of the same repository. Just chdir to the top level.
	*/

	// the name of the socket is \0 plus the git top directory
	char name[PATH_MAX+2] = "\0";

	int io[2];
	pipe(io);
	int pid = fork();
	if(pid == 0) {
		close(io[0]);
		dup2(io[1],1);
		close(io[1]);
		execlp("git",
					 "git","rev-parse", "--show-toplevel",NULL);
	}
	close(io[1]);
	ssize_t amt = read(io[0],name+1,PATH_MAX);

	close(io[0]);
	int status;
	assert(pid == waitpid(pid,&status,0));
	if(!(WIFEXITED(status) && 0 == WEXITSTATUS(status))) {
		// not in a git repository
		exit(status);
	}
	// amt + 1 is always a newline
	name[amt] = '\0';
	chdir(name+1);
	//printf("Found git dir '%s'\n",name+1);
	
	uv_pipe_t conn;
	uv_write_t writing;
	uv_pipe_init(uv_default_loop(), &conn, 1);

	void cleanup(uv_write_t* req, int status) {
		exit(0);
	}

	bool quitting = (0 == strcmp(argv[1],"--quit"));

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
				// ...client -> ...main
				size_t len = strlen(argv[0]);
				argv[0][len-6] = 's';
				argv[0][len-5] = 'e';
				argv[0][len-4] = 'r';
				argv[0][len-3] = 'v';
				argv[0][len-2] = 'e';
				argv[0][len-1] = 'r';
				name[0] = '@'; // IPC is hard...
				argv[1] = name;
				execv(argv[0],argv);
			}
			printf("starting server %d\n",pid);
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
			dest.len = strlen(argv[1])+2;
			dest.base = alloca(dest.len);
			*((u16*)dest.base) = dest.len - 2;
			memcpy(dest.base + 2, argv[1], dest.len - 2);
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
