#include <git2/repository.h>
#include <git2/errors.h>
#include <assert.h>
#include <unistd.h> // getcwd


int main(int argc, char *argv[])
{
	const char wd[PATH_MAX];
	getcwd(wd,PATH_MAX);

  git_libgit2_init();

	git_repository* repo = NULL;
	git_error_code code = git_repository_open(&repo,wd);
	if(code != 0) {
		const git_error* last = giterr_last();
		assert(last);
		puts(last->message);
		return 23;
	}
	return 0;
}
