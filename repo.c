#include "repo.h"

git_repository* repo = NULL;
void repo_init(void) {
	git_repository_open(&repo,
