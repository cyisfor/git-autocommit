#include "ops.h"
#include "check.h"
#include "activity.h"
#include "hooks.h"
#include "repo.h"
#include "ensure.h"

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
#include <error.h>

typedef uint32_t u32;

// this should be global, so that it doesn't commit several times one for each connection,
// and so that it doesn't abort committing if a connection dies
struct commit_info {
	struct event* committer;
	time_t next_commit;
	u32 lines;
	u32 words;
	u32 characters;
} ci = {};

#define BLOCKSIZE 512

static void queue_commit(CC ctx);

void just_exit() {
	exit(0);
}

#pragma GCC diagnostic ignored "-Wtrampolines"

static void commit_now(CC ctx);

bool quitting = false;

static void
on_events(struct bufferevent *conn, short events, void *ctx) {
	if(events & BEV_EVENT_ERROR) {
		perror("connection error");
	} else if(events & BEV_EVENT_EOF) {
		puts("connection closed");
	} else {
		return;
	} 
	bufferevent_free(conn);
	if(quitting) {
		event_base_loopexit(base, NULL);
	}
}

static void on_read(struct bufferevent* conn, void* udata) {
	struct evbuffer* input = bufferevent_get_input(conn);
	size_t avail = evbuffer_get_length(input);
	buffereevent_enable(conn, EV_WRITE);
	// now read all the messages we see.
	while(avail > 0) {
		char op;
		evbuffer_remove(input, &op, 1);
		--avail;
		switch(op) {
		case QUIT:
			quitting = true;
			bufferevent_write(conn, &op, 1);
			return;
		case FORCE:
			commit_now(input);
			bufferevent_write(conn, &op, 1);
			break;
		case INFO: {
			pid_t pid = getpid();
			struct info_message im = {
				pid, ci.lines, ci.words, ci.characters, ci.next_commit
			};
			bufferevent_write(conn, &im, sizeof(im));
		}
		break;
		case ADD: { 
			queue_commit();
			bufferevent_write(conn, &op, 1);
		}
		break;
		default:
			error(23,0,"bad message %d\n",ctx->buf[ctx->checked]);
		};
	}
}

static void
on_accept(struct evconnlistener *listener,
					evutil_socket_t fd, struct sockaddr *address, int socklen,
					void *ctx) {
	struct bufferevent* conn = bufferevent_socket_new(base, fd,
																										BEV_OPT_CLOSE_ON_FREE);
	buffereevent_setcb(conn, on_read, NULL, on_events, NULL);
	buffereevent_enable(conn, EV_READ);
	activity_poke();
}

static
void maybe_commit(CC ctx, u32 lines, u32 words, u32 characters);

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

static void queue_commit(CC ctx) {
	git_diff* diff = NULL;
	git_tree* headtree = NULL;

	switch(git_repository_state(repo)) {
#define DERP(e) DERP2(e)
#define DERP2(e) GIT_REPOSITORY_STATE_ ## e		
#define ONE(name)																\
			case DERP(name):													\
				printf("not auto-committing when repository in " #name " %d\n", \
							 git_repository_state(repo));															\
				return;
		ONE(MERGE);
		ONE(REVERT);
		ONE(REVERT_SEQUENCE);
		ONE(CHERRYPICK);
		ONE(CHERRYPICK_SEQUENCE);
		ONE(BISECT);
		ONE(REBASE);
		ONE(REBASE_INTERACTIVE);
		ONE(REBASE_MERGE);
		ONE(APPLY_MAILBOX);
		ONE(APPLY_MAILBOX_OR_REBASE);
		};
	
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

	u32 characters = 0;
	u32 words = 0;
	u32 lines = 0;

	/* since libgit2 is badly designed, and broken,
		 first GIT_FORCE_BINARY forces binary... with empty, blank deltas, and never calls
		 on_line, so it's like nothing changed. So binary cannot be enabled for any files
		 perceived (hardcoded) as text.
		 
		 second on_line is called /twice/ for each diff, one for the - (old_file) one for
		 the + (new_file). So you can't compare the old file to the new file, since
		 they're not both called in the same function. Using a global to store the
		 line for old_file and just assume the next call will be the + for new_file,

		 third, libgit2 won't do word, or byte diffs (since binary is broken) so
		 we'll have to manually diff the lines ourselves. This won't be accurate
		 either, since we can only guess where the lines actually differ.
		 
		 like, suppose
		 this is a tricky test
		 =>
		 this is a trip yucky test
		 
		 since my guess threshold is 3, it'll match the "tri" in tricky, yielding "p"
		 (which isn't a word), then it'll match the "cky" in tricky, yielding "yu" which
		 also isn't a word.

		 can't set the threshold too high though, like
		 "thius is a test" => "this is a test" will consider "is a test" as different,
		 since the match never gets past "us ".

		 with a binary match we'd have "u" => "" as well as the offset, so we could tell
		 what word that "u" was in.
	*/

	struct old_line {
		char* s;
		int pos;
		int l;
	};

	int on_line(const git_diff_delta *delta, /**< delta that contains this data */
							const git_diff_hunk *hunk,   /**< hunk containing this data */
							const git_diff_line *line,   /**< line data */
							void *payload) {
		struct old_line* ol = (struct old_line*)payload;
		if(line->origin == '-' && line->new_lineno == -1) {
			ol->l = line->content_len;
			ol->s = malloc(ol->l);
			ol->pos = 0;
			memcpy(ol->s,line->content,ol->l);
			return 0;
		}
		
		++lines;
		const char* l = line->content;
		size_t llen = line->content_len;
		size_t j = 0;
		bool inword = false;
		short wchars = 0;
		size_t lastw = 0;
		for(;j<llen;++j) {
			/* keep old line position saved, so we can do insertions, like:
				 this is a test
				 =>
				 this is really a test
				 
				 w/out saved position, "really a test" is the diff.
				 w/ saved, "really" is the diff.

				 this might be wrong! If you have
				 this is a test
				 =>
				 this is a totally easy test

				 the diff'll be like "otally" "a" "y" "est" since it takes t,e,s,t from ol.
				 so... check that, and only consider it a match when ol matches 2 or more, or
				 end?
			*/
			if(ol->s && ol->l > ol->pos && ol->s[ol->pos] == l[j]) {
				int newpos = ol->pos;
				while(ol->s[++newpos] == l[++j]) {
					if(newpos == ol->l) break;
				}

				// now we can see if it's a long enough match
				if(newpos == ol->l || newpos - ol->pos >= 3) {
					ol->pos = newpos;
					// skip where nothing changed.
					// we're not in a word anymore though.
					if(inword) {
						++words;
						inword = false;
						lastw = j;
					}
					continue;
				}
				/* now, hopefully the diff for
					 this is a test
					 =>
					 this is a totally easy test
					 is "totally" (neither t matches 3 "tes") then "easy" (nothing matches "t")
					 then not test, because "test" matches test.
					 
					 "totally","easy"
				*/
			}
			if(isspace(l[j])) {
				if(inword) {
					inword = 0;
					if(lastw + 1 < j) {
						void commit(void) {
							//printf("word: %d %d ",lastw,j);
							//fwrite(l+lastw,j-lastw,1,stdout);
							//fputc('\n',stdout);
							
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
				// this doesn't examine more than one line, so don't bother counting the newline.
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
		free(ol->s);
		ol->s = NULL; // just in case
		return 0;
	}

	int on_file(const git_diff_delta *delta,
							float progress,
							void *payload) {
		//printf("Um %.2lf%%\n",progress * 100);
		return 0;
	}

	
	int on_binary(
		const git_diff_delta *delta,
		const git_diff_binary *binary,
		void *payload) {
		puts("HUMMM");
		return 0;
	}


	struct old_line ol = {};
	repo_check(git_diff_foreach(diff, on_file, on_binary, NULL, on_line, &ol));
	git_diff_free(diff);
	{ char buf[0x200];
		write(1, buf, snprintf(buf,0x100,"checking lwc %d %d %d\n",lines,words,characters));
	}
	maybe_commit(ctx, lines, words, characters);
}

static void post_pre_commit(uv_async_t* handle);

static
int check(const char *path, unsigned int status_flags, void *payload) {
	int* changes = (int*)payload;
	++(*changes);
	return 0;
}

static void commit_now(struct bufferevent* conn) {
	int changes = 0;
	git_status_options opt = GIT_STATUS_OPTIONS_INIT;
	opt.show = GIT_STATUS_SHOW_INDEX_ONLY;
	git_status_foreach_ext(repo,&opt,check,&changes);
	
	if(0 == changes) {
		#define LITLEN(s) s, (sizeof(s)-1)
		write(1,LITLEN("no empty commits please.\n"));
		// no empty commits, please
		return;
	}

	struct continuation after = {
		post_pre_commit, conn
	};
	HOOK_RUN("pre-commit",after);
	// now-ish
}

static void post_pre_commit(struct bufferevent* conn) {
	bufferevent_free(conn);

	git_index* idx = NULL;
	git_signature *me = NULL;

	repo_check(git_repository_index(&idx, repo));

	repo_check(git_signature_now(&me, "autocommit", "autocommit"));
	char message[0x1000];
	// why add an arbitrary path to the git log, whatever happened to have been saved
	// back when the timer was started?
	ssize_t amt = snprintf(message,0x1000,"auto %u %u %u",
												 ci.lines, ci.words, ci.characters);
	write(1, message,amt); // stdout fileno in a weird place to stop unexpected output
	write(1, " ",1);

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
	write(1,oid_hex,GIT_OID_HEXSZ);
	write(1,"\n",1);

	git_commit_free(head);
	git_tree_free(tree);
	git_signature_free(me);

	struct continuation nothing = {};
	HOOK_RUN("post-commit",nothing);
}

static void commit_later(uv_timer_t* handle) {
	commit_now((CC)handle->data);
}

static void maybe_commit(CC ctx, u32 lines, u32 words, u32 characters) {
	/* if between 1 and 300, go between 3600 and 300s,
		 if between 300 and 600, go between 300 and 60s
		 if between 600 and 6000, go between 60 and 0 */
	double between(int x1, int x2, int y1, int y2, int x) {
		return y2 + (y1 - y2) * (x - x2) / ((float)(x1 - x2));
	}
	double chars(void) {
		if(characters == 0) {
			return 9001;
		} else if(characters < 300) {
			return between(1,300,3600,300,characters);
		} else if(characters < 600) {
			return between(300,600,300,60,characters);
		} else if(characters < 6000) {
			return between(600,6000,60,0,characters);
		} else {
			return 0;
		}
	}
	double wordsderp(void) {
		/* if between 1 and 100, 3600 to 300, if between 10 and 50, 300 to 0 */
		if(words == 0) {
			return 9001;
		} else if(words < 100) {
			return between(1,100,3600,300,words);
		} else if(words < 500) {
			return between(100,500,300,0,words);
		} else {
			return 0;
		}
	}
	double linesderp(void) {
		/* if between 1 and 10, 3600 to 60, if between 10 and 20, 60 to 0 */
		if(lines == 0) {
			return 9001;
		} else if(lines < 10) {
			return between(1,10,3600,60,lines);
		} else if(lines < 20) {
			return between(10,20,60,0,lines);
		} else {
			return 0;
		}
	}

	double d = chars();
	double test = wordsderp();
	if(d > test) d = test;
	test = linesderp();
	if(d > test) d = test;

	// don't bother waiting if it's more than an hour
	if(d >= 3600) return;
	
	time_t now = time(NULL);
	if(ci.next_commit == 0 || now > ci.next_commit || now + d < ci.next_commit) {
		// keep pushing the timer back, so we commit sooner if more changes
		char buf[0x200];
		write(1,buf,snprintf(buf,0x200,"waiting %.2f\n",d)); // weird stdout fd
		
		uv_timer_stop(&ci.committer);
		ci.next_commit = now + d;
		ci.words = words;
		ci.characters = characters;
		ci.committer.data = ctx;
		uv_timer_start((uv_timer_t*)&ci, commit_later, d * 1000 + 1, 0);
	}
}

void check_init(int sock) {
	uv_timer_init(uv_default_loop(), &ci.committer);

	activity_init();
	hooks_init();

	static uv_pipe_t server;
	uv_pipe_init(uv_default_loop(), &server, 1);
	assert(0==uv_pipe_open(&server, sock));

	assert(0==uv_listen((uv_stream_t*)&server, 0x10, on_accept));
}
