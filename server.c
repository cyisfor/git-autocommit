#include "activity.h"
#include "check.h"
#include "repo.h"

#include <assert.h>
#include <unistd.h> // getcwd
#include <sys/socket.h> // 
#include <sys/un.h> // 

#include <error.h>
#include <errno.h> 



void on_connect(uv_stream_t* server, int status) {
	assert(0==status);
	check_accept(server);
	activity_poke();
}

int main(int argc, char *argv[])
{
	repo_init();
	
	struct sockaddr_un addr = {
	sun_family: AF_UNIX,
	};

	if(argc == 2) {
		memcpy(addr.sun_path+1,argv[1],strlen(argv[1]));
	} else {
		getcwd(addr.sun_path+1,107);
	}
	addr.sun_path[0] = '\0';

	// 1 = stdout
	// 2 = message log
	dup2(1,3);
	dup2(2,1);
	// now 1,2 = log, 3 = stdout
	
	check_init();
	activity_init();
	
	uv_pipe_t server;
	uv_pipe_init(uv_default_loop(), &server, 1);
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if(0!=bind(sock,(struct sockaddr*)&addr, sizeof(addr))) {
		error(errno,errno,"Could not bind!");
	}
	assert(0==uv_pipe_open(&server, sock));
	uv_listen((uv_stream_t*)&server, 5, on_connect);
	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	return 0;
}
