#define _GNU_SOURCE
#include "repo.h"
#include <unistd.h> // chdir
#include <fcntl.h> // openat
#include <assert.h> // 
#include <stdio.h> // 
#include <errno.h> // 


git_repository* repo = NULL;
void repo_init(void) {
	// test for root ugh
	char derpd[2];
	int cwd = -1;
	for(;;) {
		if(0 == git_repository_open(&repo,".")) break;
		if(cwd == -1) {
			cwd = open(".",O_PATH|O_DIRECTORY);
			assert(cwd >= 0);
		}
		if(getcwd(derpd,2)) {
			if(errno == ERANGE) continue;
			if(derpd[0] == '/' && derpd[1] == '\0') {
				fprintf(stderr,"AC: couldn't find a git repository");
				exit(1);
			}
		}
		// go up until we find one
		assert(0 == chdir(".."));
	}
	if(cwd != -1) {
		fchdir(cwd);
		close(cwd);
	}
}
