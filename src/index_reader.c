#include "repo.h"
#include <git2/index.h>
#include <git2/tree.h>
#include <git2/diff.h> 


#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>

int on_file(const git_diff_delta *delta,
						float progress,
						void *payload) {
	git_diff* diff = (git_diff*)payload;
	printf("diff %c %s\n",git_diff_status_char(delta->status),
				 delta->old_file.path);
}
	
int main(int argc, char *argv[])
{

	repo_init(".");
	git_index* idx;
	repo_check(git_repository_index(&idx, repo));

	git_diff* diff = NULL;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	repo_check(git_diff_index_to_workdir(&diff,repo,idx,&opts));

	
	git_diff_foreach(diff, on_file,NULL,NULL,NULL,diff);
	
	struct stat buf;
	time_t imtime = 0;
	if(0==stat(".git/index",&buf)) {
		imtime = buf.st_mtime;
		printf("index mtime: %lx\n",imtime);
	}
	size_t i;
	for(i=0;i<git_index_entrycount(idx);++i) {
		const git_index_entry * e = git_index_get_byindex(idx,i);
		printf("%s %x %x %x %x\n",
					 e->path,e->flags,e->mtime.seconds,GIT_IDXENTRY_STAGE(e),
			git_index_entry_stage(e));

		if(0==stat(e->path,&buf)) {
			printf("* on disk: %lx\n",buf.st_mtime);
		}
		void check(const char* aname, time_t a, const char* bname, time_t b) {
			const char* dir;
			if(a < b) {
				dir = "BEFORE";
			} else if(a == b) {
				dir = "EQUALS";
			} else {
				dir = "AFTER";
			}
			printf("* %s %s %s\n",aname,dir,bname);
		}

		check("entry",e->mtime.seconds,"index",imtime);
		check("entry",e->mtime.seconds,"disk",buf.st_mtime);
		check("index",imtime,"disk",buf.st_mtime);
	}
	git_oid oid;
	git_index_write_tree(&oid,idx);
	git_tree* tree;
	git_tree_lookup(&tree, repo, &oid);
	printf("um %ld\n",git_tree_entrycount(tree));
	return 0;
}
