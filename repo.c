#include "repo.h"

git_repository* repo = NULL;
void repo_init(void) {
	int cwd = -1;
	for(;;) {
		if(0 == git_repository_open(&repo,".")) break;
		if(cwd == -1) {
			cwd = open(".",O_PATH|O_DIRECTORY);
			assert(cwd >= 0);
		}
		// go up until we find one
		assert(0 == chdir(".."));
	}
	if(cwd != -1) {
		fchdir(cwd);
		close(cwd);
	}
}
