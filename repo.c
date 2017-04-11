#define _GNU_SOURCE
#include "repo.h"

#include <unistd.h> // chdir
#include <fcntl.h> // openat
#include <assert.h> // 
#include <stdio.h> // 
#include <errno.h> // 

git_repository* repo = NULL;
#ifdef RETURN_STUPIDLY
const char repo_path[PATH_MAX];
#endif

int repo_init(void) {
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
				return 1;
			}
		}
		// go up until we find one
		assert(0 == chdir(".."));
	}
#ifdef RETURN_STUPIDLY
	assert(NULL != realpath(".",repo_path));
	if(cwd != -1) {
		fchdir(cwd);
		close(cwd);
	}
#endif
	return 0;
}

void repo_check(git_error_code e) {
	if(e == 0) return;
	const git_error* err = giterr_last();
	if(err != NULL) {
		fprintf(stderr,"GIT ERROR: %s\n",err->message);
		giterr_clear();
	}
	
	exit(e);
}
