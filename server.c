#include "activity.h"
#include "check.h"
#include "repo.h"
#include "net.h"

#include <assert.h>
#include <unistd.h> // getcwd
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>



#include <error.h>
#include <errno.h> 

int main(int argc, char *argv[])
{
	assert(0 == repo_init());

	int sock;
	bool unbound = getenv("bound")==NULL;
	if(unbound) {
		net_set_addr();
		sock = net_bind();
		if(sock == -1) exit(1);
	} else {
		sock = 3;
		// we shall not be bound
		unbound = true;
	}

	check_init();
	activity_init();
	
	uv_pipe_t server;
	uv_pipe_init(uv_default_loop(), &server, 1);
	assert(0==uv_pipe_open(&server, sock));

	void acceptit(int status) {
		assert(0==status);
		check_accept((uv_stream_t*)&server);
	}
	
	void on_connect(uv_stream_t* neh, int status) {
		acceptit(status);
	}

	void on_polled(uv_poll_t* handle, int status, int events) {
		acceptit(status);
	}

	// is it bad to listen() twice? should I uv_poll instead?
	if(unbound) {
		uv_listen((uv_stream_t*)&server, 0x10, on_connect);
	} else {
		uv_poll_t poll;
		uv_poll_init_socket(uv_default_loop(), &poll, sock);
		uv_poll_start(&poll, UV_READABLE, on_polled);
	}
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	return 0;
}
