#define _GNU_SOURCE
#include "repo.h"

#include <git2/global.h>

#include <unistd.h> // chdir
#include <fcntl.h> // openat
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h> // strlen

git_repository* repo = NULL;
#ifdef RETURN_STUPIDLY
const char repo_path[PATH_MAX];
#endif

int repo_discover_init(char* start, int len) {
	/* find a git repository that the file start is contained in.	*/
	struct stat st;
	assert(0==stat(start,&st));
	char* end = NULL;
	char save;
	if(!S_ISDIR(st.st_mode)) {
		end = start + len - 1;
		assert(end != start);
		while(end > start) {
			if(*end == '/') {
				save = *end;
				*end = '\0';
				break;
			}
			--end;
		}
		if(end == start) {
			start = ".";
			end = NULL;
		}
	}
	git_libgit2_init();
	int res = git_repository_open_ext(&repo,
																		start,
																		0,
																		NULL);
	if(end != NULL) {
		*end = save;
	}
	return res;
}

int repo_init(const char* start) {
	return git_repository_open(&repo, start);
}

size_t repo_relative(char** path, size_t plen) {
	const char* workdir = git_repository_workdir(repo);
	assert(workdir != NULL); // we can't run an editor on a bare repository!
	size_t len = strlen(workdir);
	assert(len < plen);
	*path = *path + len;
	return plen - len;
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
