#include <git2/repository.h>
#include <git2/errors.h>
#include <assert.h>

int main(int argc, char *argv[])
{
	git_repository* repo = NULL;
	if(0 != git_repository_open(&repo,getcwd())) {
		puts(giterr_last()->message);
		return 23;
	}
	return 0;
}
