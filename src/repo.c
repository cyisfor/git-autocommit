#define _GNU_SOURCE
#include "repo.h"

#include <git2/global.h>
#include <git2/index.h>

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

/* ugh, this is messy... so we have a file like a/b/c/d.txt and a repo in a/b/.git/
	 except our current directory is a/
	 so we call file=./b/c/d.txt ./client and it doesn't know wtf to do with ./b/c/d.txt

./b/c/d.txt is not a dir, so
=>
./b/c\0d.txt
(if there are no slashes in it, just go to ".")

now pass ./b/c to git_repository_open_ext
(we can't just git_repository_discover, since we need the workdir)
now restore ./b/c/d.txt, but only if there were slashes, otherwise we're restoring uninitialized memory

then (maybe) fork the server

then later get the workdir /a/b/
then absolutize ./b/c/d.txt
=>
/a/b/c/d.txt
then remove the /a/b/ prefix
=>
c/d.txt
send c/d.txt to the server

*/

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

void repo_add(const char* path) {
	git_index* idx;
	repo_check(git_repository_index(&idx, repo));
	git_index_read(idx, 1);
	assert(idx);
	assert(path);
	
	// don't repo_check b/c this fails if already added
	if(0 == git_index_add_bypath(idx, path)) {
		printf("added path %s\n",path);
		git_index_write(idx);
	} else {
		printf("error adding path %s\n",path);
	}
	git_index_free(idx);
}
