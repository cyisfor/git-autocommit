#define _GNU_SOURCE
#include "repo.h"

#include <git2/global.h>



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
	git_libgit2_init();

	return git_repository_open_ext(&repo,
																		".",
																		0,
																		NULL);
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
