#include "repo.h"
#include <git2/index.h>
#include <git2/tree.h> // 


#include <assert.h>
#include <stdio.h> 





int main(int argc, char *argv[])
{
	
	repo_init();
	git_index* idx;
	repo_check(git_repository_index(&idx, repo));
	size_t i;
	for(i=0;i<git_index_entrycount(idx);++i) {
		const git_index_entry * e = git_index_get_byindex(idx,i);
		printf("hmm %s %x %x\n",e->path,e->flags,e->flags_extended);
	}
	git_oid oid;
	git_index_write_tree(&oid,idx);
	git_tree* tree;
	git_tree_lookup(&tree, repo, &oid);
	printf("um %d\n",git_tree_entrycount(tree));
	return 0;
}
