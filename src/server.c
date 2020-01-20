#include "eventbase.h"

#include "activity.h"
#include "check.h"
#include "repo.h"
#include "net.h"
#include "note.h"

#include <assert.h>
#include <unistd.h> // getcwd
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <sys/prctl.h>

int main(int argc, char *argv[])
{
	if(argc == 1) {
		assert(0 == repo_discover_init(".",1));
	} else {
		assert(0 == repo_init(argv[1]));
	}
	note_init();
	// no overflow why?
	strcpy(argv[0], "autocommit server (standalone)");
	prctl(PR_SET_NAME, "autocommit server (standalone)", 0, 0, 0);
	net_set_addr();
	int sock = net_bind();
	if(sock == -1) exit(1);
	struct event_base* eventbase = eventbase_init();
	check_init(eventbase, sock);

	event_base_dispatch(eventbase);

	return 0;
}
