#include "check.h"

void on_connect(uv_stream_t* server, int status) {
	assert_zero(status);
	check_accept(server);
}

int main(int argc, char *argv[])
{
	uv_tcp_t server;
	uv_tcp_init(&server);
	struct sockaddr_in addr;
	uv_ip4_addr("127.0.0.1", 9523, &addr);
	uv_tcp_bind(&server, &addr);
	uv_listen(&server, 5, on_connect);
	uv_run(uv_default_loop, UV_RUN_DEFAULT);
	return 0;
}
