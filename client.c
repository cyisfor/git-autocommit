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

	void on_connect(uv_connect_t* req, int status) {
		assert(status == 0);
		uv_buf_t dest;
		dest.len = strlen(argv[1])+2;
		dest.base = alloca(dest.len);
		*((u16*)dest.base) = dest.len - 2;
		memcpy(dest.base + 2, argv[1], dest.len - 2);
		uv_write(&writing, (uv_stream_t*) &conn, &dest, 1, cleanup);
	}
	
	uv_tcp_connect(&derp, &conn, (const struct sockaddr*) &addr, on_connect);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	return 0;
}
