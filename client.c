#include <uv.h>
#include <stdlib.h> // exit
#include <stdint.h>
#include <assert.h>
#include <string.h> // strlen
#include <unistd.h> // fork, execv



typedef uint16_t u16;

int main(int argc, char *argv[])
{
	uv_pipe_t conn;
	uv_write_t writing;
	uv_pipe_init(uv_default_loop(), &conn, 1);

	void cleanup(uv_write_t* req, int status) {
		exit(0);
	}

	int tries = 0;
	uv_timer_t trying;
	uv_timer_init(uv_default_loop(),&trying);

	void on_connect(uv_connect_t* req, int status) {
		if(status < 0) {
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
				execv(argv[0],argv);
			}
			printf("starting server %d\n",pid);
			return;
		}
		tries = 0;
		uv_timer_stop(&trying);

		uv_buf_t dest;
		dest.len = strlen(argv[1])+2;
		dest.base = alloca(dest.len);
		*((u16*)dest.base) = dest.len - 2;
		memcpy(dest.base + 2, argv[1], dest.len - 2);
		uv_write(&writing, (uv_stream_t*) &conn, &dest, 1, cleanup);
	}

	uv_connect_t derp;
	void try_connect() {
		char name[0x200] = "\0";
		assert(getcwd(name+1,0x200-1));
		uv_pipe_connect(&derp, &conn, name, on_connect);
	}
	uv_timer_start(&trying, try_connect, 0, 200);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	return 0;
}
