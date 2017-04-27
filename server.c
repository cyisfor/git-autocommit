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
	if(argc == 1) {
		assert(0 == repo_discover_init(".",1));
	} else {
		assert(0 == repo_init(argv[1]));
	}

	net_set_addr();
	int sock = net_bind();
	if(sock == -1) exit(1);

	check_run(sock);
	return 0;
}
