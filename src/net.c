#define _GNU_SOURCE
#include "net.h"
#include "repo.h"
#include "min.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>


// the name of the socket is \0 plus the git top directory
static struct sockaddr_un addr = {
sun_family: AF_UNIX,
sun_path: "\0" // "abstract" sockets
};

void net_set_addr(void) {
	const char* path = git_repository_path(repo);
	memcpy(addr.sun_path+1, path, MIN(107,strlen(path)));
}

int net_bind(void) {
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if(0!=bind(sock,(struct sockaddr*)&addr, sizeof(addr))) {
		if(errno != EADDRINUSE)
			error(errno,errno,"bind failed");
		return -1;
	}
/*	if(0!=listen(sock, 0x10))
		error(errno,errno,"listen failed");
*/
	fcntl(sock,F_SETFL,fcntl(sock,F_GETFL) | O_NONBLOCK);
	return sock;
}

int net_connect(void) {
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	fcntl(sock,F_SETFL,fcntl(sock,F_GETFL) | O_NONBLOCK);
	if(0 != connect(sock,(struct sockaddr*)&addr,sizeof(addr))) {
		if(errno == ECONNREFUSED)
			return -1;
		error(errno,errno,"connect failed");
	}
	fcntl(sock,F_SETFL,fcntl(sock,F_GETFL) | O_NONBLOCK);
	return sock;
}

static struct ucred ucred;
static void getcred(int sock) {
	static bool gotcred = false;
	if(gotcred) return;
	int len = sizeof(ucred);
	assert(0 == getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &ucred, &len));
	gotcred = true;
}

/* if you bind a unix socket, then pass it to a child process, then close the socket in the
	 parent process, then connect to the socket from parent to child, then SO_PEERCRED will
	 still think the socket's owned by the parent. Only when the parent process dies is this
	 corrected. So if we forked, just use that pid, ignoring SO_PEERCRED
*/

static pid_t forkhack = -1;
void net_forkhack(pid_t pid) {
	forkhack = pid;
}

pid_t net_pid(int sock) {
	if(forkhack != -1)
		return forkhack;
	getcred(sock);	
	return ucred.pid;
}
