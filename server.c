#include "activity.h"
#include "check.h"
#include <assert.h>
#include <unistd.h> // getcwd


void on_connect(uv_stream_t* server, int status) {
	assert(0==status);
	check_accept(server);
	activity_poke();
}

int main(int argc, char *argv[])
{
	assert(argc == 2); // argv[1] is the socket name sorta (\1 to avoid early arg end)
	char* name = argv[1];
	name[0] = '\0';

	// 1 = stdout
	// 2 = message log
	dup2(1,3);
	dup2(2,1);
	// now 1,2 = log, 3 = stdout
	
	check_init();
	activity_init();
	uv_pipe_t server;
	uv_pipe_init(uv_default_loop(), &server, 1);
	assert(getcwd(name+1,0x200-1));
	uv_pipe_bind(&server, name);
	uv_listen((uv_stream_t*)&server, 5, on_connect);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	return 0;
}
