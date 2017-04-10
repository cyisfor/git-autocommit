#include "check.h"
#include <assert.h>
#include <unistd.h> // getcwd


void on_connect(uv_stream_t* server, int status) {
	assert(0==status);
	check_accept(server);
}

int main(int argc, char *argv[])
{
	uv_pipe_t server;
	uv_pipe_init(uv_default_loop(), &server, 1);
	char name[0x200] = "\0"; // "abstract" sockets are way less messy
	assert(getcwd(name+1,0x200-1));
	uv_pipe_bind(&server, name);
	uv_listen((uv_stream_t*)&server, 5, on_connect);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	return 0;
}
