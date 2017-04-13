#include "check.h"
#include "activity.h"

#include "repo.h"

#include <git2/diff.h>
#include <git2/refs.h>
#include <git2/tree.h>
#include <git2/index.h>
#include <git2/commit.h>
#include <git2/signature.h>
#include <git2/status.h>

#include <assert.h>
#include <stdlib.h> // malloc
#include <string.h> // memcpy
#include <stdio.h> // snprintf
#include <unistd.h> // execvp
#include <sys/mman.h> // mmap
#include <stdint.h> // uint*
#include <sys/wait.h> // waitpid
#include <ctype.h> // isspace
#include <stdbool.h>


typedef int32_t i32;

struct check_context {
	uv_tcp_t stream;
	size_t checked; // parsed paths up to here
	size_t read; // chars read so far
	size_t space; // space in buffer
	char* buf;
};

#define BLOCKSIZE 512

static void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* ret) {
	CC ctx = (CC) handle;
	size_t unchecked = ctx->read - ctx->checked;
	if(unchecked < ctx->checked) {
		// consolodate by removing old stuff (not overlapping)
		size_t canremove = (ctx->read / BLOCKSIZE) * BLOCKSIZE;
		if(canremove > 0) {
			memcpy(ctx->buf, ctx->buf + canremove, unchecked);
			ctx->checked = 0;
			ctx->read = unchecked;
			ctx->space -= canremove;
			// should be able to reuse instead of shrinking
			//ctx->buf = realloc(ctx->buf, ctx->space);
		}
	}
	if(ctx->read + size > ctx->space) {
		ctx->space += BLOCKSIZE;
		assert(ctx->space > 0);
		ctx->buf = realloc(ctx->buf, ctx->space);
	}
	ret->base = ctx->buf + ctx->read;
	ret->len = ctx->space - ctx->read;
}

static void cleanup(uv_handle_t* h) {
	CC ctx = (CC) h;
	free(ctx->buf);
	free(ctx);
	//puts("cleaned up");
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
	CC ctx = (CC) stream;
	if(nread < 0 || nread == UV_EOF) {
		uv_close((uv_handle_t*)stream, cleanup);
		return;
	}
	ctx->read += nread;

	// now read all the paths we see.
	for(;;) {
		// break if there isn't even a size to be read
		if(ctx->read < ctx->checked + 2) break;
		// no ntohs needed since it'd be silly not to run client/server on the same machine.
		u16 size = *((u16*)(ctx->buf + ctx->checked));
		if(size == 0) {
			// special message incoming
			
			// the message hasn't finished coming in yet, break
			if(ctx->read < ctx->checked + 3) break;
			
			switch(ctx->buf[ctx->checked+2]) {
			case 0:
				exit(0);
			};
			ctx->checked += 3; // size plus message
			continue;
		}
		// the path hasn't finished coming in yet, break
		if(ctx->read < ctx->checked + 2 + size) break;
		char* path = malloc(size+1); // +1 for the null
		memcpy(path, ctx->buf + ctx->checked + 2, size);
		path[size] = '\0';
		check_path(ctx, path,size);
		ctx->checked += 2 + size;
	}
}

void check_accept(uv_stream_t* server) {
	CC ctx = (CC) malloc(sizeof(struct check_context));
	uv_tcp_init(uv_default_loop(), &ctx->stream);
	int res = uv_accept(server, (uv_stream_t*) &ctx->stream);
	if(res == UV_EAGAIN) {
		free(ctx);
		puts("ugh");
		return;
	}
	if(res != 0) {
		error(-res,-res,"um");
	}
	ctx->buf = NULL;
	ctx->space = ctx->read = ctx->checked = 0;
	uv_read_start((uv_stream_t*)ctx, alloc_cb, on_read);
	activity_poke();
}

static void maybe_commit(CC ctx, char* path, i32 lines, i32 words, i32 characters);

// don't cache the head, because other processes could commit to the repository while we're running!
static git_commit* get_head(void) {
	git_reference* master = NULL;
	git_reference* derp = NULL;
	git_commit* head = NULL;
	repo_check(git_repository_head(&master, repo));
	//printf("umm %s\n",git_reference_shorthand(master));
	repo_check(git_reference_resolve(&derp, master));
		
	//printf("umm %s\n",git_reference_shorthand(derp));
	const git_oid *oid = git_reference_target(derp);
	assert(oid != NULL);
	char boop[GIT_OID_HEXSZ];
	git_oid_fmt(boop, oid);
	//printf("OID %s\n",boop);

	repo_check(git_commit_lookup(&head, repo, oid));

	git_reference_free(derp);
	git_reference_free(master);
	return head;
}

void check_path(CC ctx, char* path, u16 len) {
	git_index* idx;
	repo_check(git_repository_index(&idx, repo));
	git_index_read(idx, 1);
	// don't repo_check b/c this fails if already added
	assert(idx);
	assert(path);
	if(0 == git_index_add_bypath(idx, path)) {
		git_index_write(idx);
	}
	git_index_free(idx);

	git_diff* diff = NULL;
	git_tree* headtree = NULL;

	git_commit* head = get_head();
	repo_check(git_commit_tree(&headtree, head));	
	git_commit_free(head);

	git_diff_options options = GIT_DIFF_OPTIONS_INIT;
	options.context_lines = 0;
	options.flags =
		GIT_DIFF_IGNORE_SUBMODULES |
		GIT_DIFF_SKIP_BINARY_CHECK |
		GIT_DIFF_IGNORE_WHITESPACE;
	
	repo_check(git_diff_tree_to_workdir_with_index(
							 &diff,
							 repo,
							 headtree,
							 &options));
	git_tree_free(headtree);

	i32 characters = 0;
	i32 words = 0;
	i32 lines = 0;

	int on_line(const git_diff_delta *delta, /**< delta that contains this data */
							const git_diff_hunk *hunk,   /**< hunk containing this data */
							const git_diff_line *line,   /**< line data */
							void *payload) {
		++lines;
		const char* l = line->content;
		size_t llen = line->content_len;
		size_t j = 0;
		bool inword = true;
		short wchars = 0;
		size_t lastw = 0;
		for(;j<llen;++j) {
			if(isspace(l[j])) {
				if(inword) {
					inword = 0;
					if(lastw + 1 < j) {
						void commit(void) {
							/*
								printf("word: %d %d ",lastw,j);
								fwrite(diff+lastw,j-lastw,1,stdout);
								fputc('\n',stdout);
							*/
							if(wchars > 0) {
								++words;
								wchars = 0;
							}
						}
						if(lastw + 2 == j) {
							// 1 letter
							switch(l[lastw]) {
							case 'a':
							case 'A':
							case 'i':
							case 'I':
							case 'u':
							case 'U':
							case 'y':
							case 'Y':
								break;
							default:
								commit();
							};
						} else {
							commit();
						}
						lastw = j;
					}
				} else {
					lastw = j;
				}
				if(l[j] == '\n')
					break;
			} else {
				if(!inword) {
					inword = 1;
					lastw = j;
				}
			}
			if(isalnum(l[j])) {
				++wchars;
			}
			++characters;
		}			
		return 0;
	}

	int on_file(const git_diff_delta *delta,
							float progress,
							void *payload) {
		//printf("Um %.2lf%%\n",progress * 100);
		return 0;
	}
	

	repo_check(git_diff_foreach(diff, on_file, NULL, NULL, on_line, NULL));
	git_diff_free(diff);
	maybe_commit(ctx, path, lines, words, characters);
}


static void commit_now(CC ctx, char* path, i32 lines, i32 words, i32 characters) {
	git_signature *me = NULL;
	git_index* idx = NULL;
	git_diff* diff;
	int changes = 0;
	int check(const char *path, unsigned int status_flags, void *payload) {
		++changes;
		return 0;
	}
	git_status_options opt = GIT_STATUS_OPTIONS_INIT;
	opt.show = GIT_STATUS_SHOW_INDEX_ONLY;
	git_status_foreach_ext(repo,&opt,check,NULL);
	
	if(0 == changes) {
		#define LITLEN(s) s, (sizeof(s)-1)
		write(3,LITLEN("AC: no empty commits please.\n"));
		// no empty commits, please
		return;
	}
	repo_check(git_repository_index(&idx, repo));

	repo_check(git_signature_now(&me, "autocommit", "autocommit"));
	char message[0x1000];
	ssize_t amt = snprintf(message,0x1000,"auto (%s) %lu %lu %lu",
												 path, lines, words, characters);
	write(3, "AC: ",4);
	write(3, message,amt); // stdout fileno in a weird place to stop unexpected output
	write(3, " ",1);

	git_oid treeoid;
	repo_check(git_index_write_tree(&treeoid, idx));
	git_tree* tree = NULL;
	repo_check(git_tree_lookup(&tree, repo, &treeoid));

	git_oid new_commit;

	git_commit* head = get_head();

	repo_check(git_commit_create(
							 &new_commit,
							 repo,
							 "HEAD", /* name of ref to update */
							 me, /* author */
							 me, /* committer */
							 "UTF-8", /* message encoding */
							 message,  /* message */
							 tree, /* root tree */
							 1,                           /* parent count */
							 (const git_commit**) &head)); /* parents */

	git_index_write(idx);
	git_index_free(idx);

	
	char oid_hex[GIT_OID_HEXSZ+1] = { 0 };
	git_oid_fmt(oid_hex, &new_commit);
	write(3,oid_hex,GIT_OID_HEXSZ);
	write(3,"\n",1);

	git_commit_free(head);
	git_tree_free(tree);
	git_signature_free(me);
	
	free(path); // won't need this after the commit is done...
}

// this should be global, so that it doesn't commit several times one for each connection,
// and so that it doesn't abort committing if a connection dies
struct commit_info {
	uv_timer_t committer;
	time_t next_commit;
	char* path;
	i32 lines;
	i32 words;
	i32 characters;
} ci = {};

void check_init(void) {
	uv_timer_init(uv_default_loop(), &ci.committer);
}

static void commit_later(uv_timer_t* handle) {
	commit_now((CC)handle->data, ci.path, ci.lines, ci.words, ci.characters);
	ci.path = NULL; // just in case
}

static void maybe_commit(CC ctx, char* path, i32 lines, i32 words, i32 characters) {
	/* if between 1 and 30, go between 3600 and 300s,
		 if between 30 and 60, go between 300 and 60s
		 if between 60 and 600, go between 60 and 0 */
	double between(int x1, int x2, int y1, int y2, int x) {
		return y2 + (y1 - y2) * (x - x2) / ((float)(x1 - x2));
	}
	double chars(void) {
		if(characters >= 1) {
			if(characters < 30) {
				return between(1,30,3600,300,characters);
			} else if(characters < 60) {
				return between(30,60,300,60,characters);
			} else if(characters < 600) {
				return between(60,600,60,0,characters);
			} else {
				return 0;
			}
		}
		return 9001;
	}
	double wordsderp(void) {
		/* if between 1 and 10, 3600 to 60, if between 10 and 50, 60 to 0 */
		if(words >= 1) {
			if(words < 10) {
				return between(1,10,3600,60,words);
			} else if(words < 50) {
				return between(10,50,60,0,words);
			} else {
				return 0;
			}
		}
		return 9001;
	}
	double linesderp(void) {
		/* if between 1 and 5, 3600 to 60, if between 5 and 10, 60 to 0 */
		if(lines >= 1) {
			if(lines < 5) {
				return between(1,5,3600,60,lines);
			} else if(lines < 10) {
				return between(5,10,60,0,lines);
			} else {
				return 0;
			}
		}
		return 9001;
	}

	double d = chars();
	double test = wordsderp();
	if(d > test) d = test;
	test = linesderp();
	if(d > test) d = test;

	// don't bother waiting if it's more than an hour
	if(d >= 3600) return;
	if(d <= 1) {
		uv_timer_stop(&ci.committer);
		commit_now(ctx,path,lines,words,characters);
	} else {
		time_t now = time(NULL);
		if(ci.next_commit == 0 || now + d < ci.next_commit) {
			// keep pushing the timer back, so we commit sooner if more changes
			char buf[0x200];
			write(3,buf,snprintf(buf,0x200,"AC: %s waiting %.2f\n",path,d)); // weird stdout fd

			uv_timer_stop(&ci.committer);
			ci.next_commit = now + d;
			ci.path = path;
			ci.words = words;
			ci.characters = characters;
			uv_timer_start((uv_timer_t*)&ci, commit_later, d * 1000, 0);
		}
	}
}
