#include <uv.h>
#include <stdlib.h> // exit
#include <stdint.h>
#include <assert.h>
#include <string.h> // strlen


typedef uint16_t u16;

int main(int argc, char *argv[])
{
	uv_tcp_t conn;
	uv_write_t writing;
	uv_tcp_init(uv_default_loop(), &conn);
	struct sockaddr_in addr;
	uv_ip4_addr("127.0.0.1", 9523, &addr);
	uv_connect_t derp;

	void cleanup(uv_write_t* req, int status) {
		exit(0);
	}

	int tries = 0;
	uv_timer_t trying;
	uv_timer_init(uv_default_loop(),&trying);

	void on_connect(uv_connect_t* req, int status) {
		if(status < 0) {
			if(tries != 3) return;
			// start the server
			int pid = fork();
			if(pid == 0) {
				// ...client -> ...main
				puts("starting server...");
				size_t len = strlen(argv[0]);
				argv[len-6] = 'm';
				argv[len-5] = 'a';
				argv[len-4] = 'i';
				argv[len-3] = 'n';
				argv[len-2] = '\0';
				execv(argv[0],argv);
			}			
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

	void try_connect() {
		uv_tcp_connect(&derp, &conn, (const struct sockaddr*) &addr, on_connect);
	}
	uv_timer_start(&trying, try_connect, 0, 1000);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	return 0;
}
