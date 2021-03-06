#include "ignore.h"
#include "eventbase.h"
#include "ops.h"
#include "check.h"
#include "activity.h"
#include "hooks.h"
#include "repo.h"
#include "ensure.h"

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>

#include <git2/diff.h>
#include <git2/refs.h>
#include <git2/tree.h>
#include <git2/index.h>
#include <git2/commit.h>
#include <git2/signature.h>
#include <git2/status.h>
#include <git2/buffer.h>

#include <gpgme.h>

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
#include <err.h>

gpgme_ctx_t gpgme_ctx = NULL;

static
void gpg_check(int res) {
	if(res == GPG_ERR_NO_ERROR) return;
	fprintf(stderr, "GPG failed %s %s\n",
			gpgme_strerror(res),
			gpgme_strsource(res));
	abort();
}

typedef uint32_t u32;

// this should be global, so that it doesn't commit several times one for each connection,
// and so that it doesn't abort committing if a connection dies
struct commit_info {
	struct event* later;
	time_t next_commit;
	u32 lines;
	u32 words;
	u32 characters;
} ci = {};

#define BLOCKSIZE 512

static void queue_commit(void);

void just_exit() {
	exit(0);
}

#pragma GCC diagnostic ignored "-Wtrampolines"

struct commit_later_data {
	struct bufferevent* conn;
	struct event_base* eventbase;
};

static void commit_now(struct commit_later_data* data);

bool quitting = false;

static void
on_events(struct bufferevent *conn, short events, void *udata) {
	struct event_base* eventbase = (struct event_base*)udata;
	if(events & BEV_EVENT_ERROR) {
		perror("connection error");
	} else if(events & BEV_EVENT_EOF) {
		puts("connection closed");
	} else {
		return;
	} 
	bufferevent_free(conn);
	if(quitting) {
		event_base_loopexit(eventbase, NULL);
	}
}

static void on_read(struct bufferevent* conn, void* udata) {
	struct event_base* eventbase = (struct event_base*)udata;
	struct evbuffer* input = bufferevent_get_input(conn);
	size_t avail = evbuffer_get_length(input);
	bufferevent_enable(conn, EV_WRITE);
	// now read all the messages we see.
	while(avail > 0) {
		enum operations op = OP_QUIT;
		evbuffer_remove(input, &op, 1);
		--avail;
		switch(op) {
		case OP_QUIT:
			quitting = true;
			record(INFO, "quitting");
			bufferevent_write(conn, &op, 1);
			return;
		case OP_FORCE: {
			struct commit_later_data* data = malloc(sizeof(struct commit_later_data));
			data->conn = conn;
			data->eventbase = eventbase;
			commit_now(data);
			bufferevent_write(conn, &op, 1);
		}
			break;
		case OP_INFO: {
			pid_t pid = getpid();
			struct info_message im = {
				pid, ci.lines, ci.words, ci.characters, ci.next_commit
			};
			bufferevent_write(conn, &im, sizeof(im));
		}
		break;
		case OP_ADD: { 
			queue_commit();
			bufferevent_write(conn, &op, 1);
		}
		break;
		default:
			warnx("bad message %d\n",op);
		};
	}
}

static void
on_accept(struct evconnlistener *listener,
					evutil_socket_t fd, struct sockaddr *address, int socklen,
					void *udata) {
	struct event_base* eventbase = (struct event_base*)udata;
	struct bufferevent* conn = bufferevent_socket_new(
		eventbase, fd,
		BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(conn, on_read, NULL, on_events, eventbase);
	bufferevent_enable(conn, EV_READ);
	activity_poke();
}

static
void maybe_commit(u32 lines, u32 words, u32 characters);

// don't cache the head, because other processes could commit to the repository while we're running!
static git_commit* get_head(git_reference* headref) {
	git_reference* derp = NULL;
	git_commit* head = NULL;
	//printf("umm %s\n",git_reference_shorthand(master));
	repo_check(git_reference_resolve(&derp, headref));
		
	//printf("umm %s\n",git_reference_shorthand(derp));
	const git_oid *oid = git_reference_target(derp);
	assert(oid != NULL);
	char boop[GIT_OID_HEXSZ];
	git_oid_fmt(boop, oid);
	//printf("OID %s\n",boop);

	repo_check(git_commit_lookup(&head, repo, oid));

	git_reference_free(derp);
	return head;
}

static void queue_commit(void) {
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

	git_reference* headref = NULL;
	repo_check(git_repository_head(&headref, repo));
	git_commit* head = get_head(headref);
	repo_check(git_commit_tree(&headtree, head));
	git_reference_free(headref);	
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
		ignore(write(1, buf, snprintf(buf,0x100,"checking lwc %d %d %d\n",lines,words,characters)));
	}
	maybe_commit(lines, words, characters);
}

static void post_pre_commit(void* udata);

static
int check(const char *path, unsigned int status_flags, void *payload) {
	int* changes = (int*)payload;
	++(*changes);
	return 0;
}

static void commit_now(struct commit_later_data* data) {
	int changes = 0;
	git_status_options opt = GIT_STATUS_OPTIONS_INIT;
	opt.show = GIT_STATUS_SHOW_INDEX_ONLY;
	git_status_foreach_ext(repo,&opt,check,&changes);
	
	if(0 == changes) {
		ignore(write(1,LITLEN("no empty commits please.\n")));
		// no empty commits, please
		return;
	}

	struct continuation after = {
		.eventbase = data->eventbase,
		.func = post_pre_commit,
		.arg = data
	};
	HOOK_RUN(data->eventbase, "pre-commit",after);
	// now-ish
}

static void post_pre_commit(void* udata) {
	struct commit_later_data* data = (struct commit_later_data*) udata;
	bufferevent_free(data->conn);

	git_index* idx = NULL;
	git_signature *me = NULL;

	repo_check(git_repository_index(&idx, repo));

	repo_check(git_signature_now(&me, "autocommit", "autocommit"));
	char message[0x1000];
	// why add an arbitrary path to the git log, whatever happened to have been saved
	// back when the timer was started?
	ssize_t amt = snprintf(message,0x1000,"auto %u %u %u",
												 ci.lines, ci.words, ci.characters);
	ignore(write(1, message,amt)); // stdout fileno in a weird place to stop unexpected output
	ignore(write(1, " ",1));

	git_oid treeoid;
	repo_check(git_index_write_tree(&treeoid, idx));
	git_tree* tree = NULL;
	repo_check(git_tree_lookup(&tree, repo, &treeoid));

	git_oid new_commit;

	git_reference* headref = NULL;
	repo_check(git_repository_head(&headref, repo));
	git_commit* head = get_head(headref);

	git_buf commit_content = {};
	repo_check(git_commit_create_buffer(
							 &commit_content,
							 repo,
							 me, /* author */
							 me, /* committer */
							 "UTF-8", /* message encoding */
							 message,  /* message */
							 tree, /* root tree */
							 1,                           /* parent count */
							 (const git_commit**) &head)); /* parents */
	assert(strlen(commit_content.ptr) == commit_content.size);
	/* now sign the contents with gpgme */
	gpgme_data_t gpgcommit;
	gpg_check(gpgme_data_new_from_mem(
		&gpgcommit, 
		commit_content.ptr,
		commit_content.size,
		0));
	gpgme_data_t gpgsig;
	gpg_check(gpgme_data_new(&gpgsig));

	const char* fpr = getenv("AUTOCOMMIT_KEY");
	ensure_ne(NULL, fpr);
	gpgme_key_t key;
	gpg_check(gpgme_get_key(gpgme_ctx, fpr, &key, 1));
	gpg_check(gpgme_signers_add(gpgme_ctx, key));
	
	gpg_check(gpgme_op_sign(gpgme_ctx, gpgcommit,
							gpgsig, GPGME_SIG_MODE_DETACH));

	gpgme_signers_clear(gpgme_ctx);
	gpgme_data_release(gpgcommit);
	
	size_t gpgsiglen = 0;
	char* gpgsigdata = gpgme_data_release_and_get_mem(gpgsig, &gpgsiglen);
	/* XXX: this won't buffer overrun, will it? */
	gpgsigdata[gpgsiglen] = 0;
	
	repo_check(git_commit_create_with_signature(
				   &new_commit,
				   repo,
				   commit_content.ptr,
				   gpgsigdata,
				   "gpgsig"));

	gpgme_free(gpgsigdata);

	repo_check(git_reference_set_target(
				   &headref,
				   headref,
				   &new_commit,
				   "automatic commit"));

	git_reference_free(headref);

	git_index_write(idx);
	git_index_free(idx);

	
	char oid_hex[GIT_OID_HEXSZ+1] = { 0 };
	git_oid_fmt(oid_hex, &new_commit);
	ignore(write(1,oid_hex,GIT_OID_HEXSZ));
	ignore(write(1,"\n",1));

	git_commit_free(head);
	git_tree_free(tree);
	git_signature_free(me);

	struct continuation nothing = {};
	HOOK_RUN(data->eventbase, "post-commit",nothing);
	free(data);
}

static void commit_later(evutil_socket_t nope, short events, void *arg) {
	commit_now(arg);
}

static void maybe_commit(u32 lines, u32 words, u32 characters) {
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
		ignore(write(1,buf,snprintf(buf,0x200,"waiting %.2f\n",d)));
// weird stdout fd
		
		evtimer_del(ci.later);
		ci.next_commit = now + d;
		ci.words = words;
		ci.characters = characters;

		const struct timeval timeout = {
			.tv_sec = d + 1
		};
		evtimer_add(ci.later,&timeout);
	}
}

static
void listen_error(struct evconnlistener * listener, void * ctx) {
	struct event_base *eventbase = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();
	{ char buf[0x1000];
		ignore(write(1,buf,snprintf(
									 buf, 0x1000,
									 "accept error %d (%s) halting in 5s\n",
									 err,
									 evutil_socket_error_to_string(err))));
	}
	evconnlistener_free(listener);
	sleep(5);
	event_base_loopexit(eventbase, NULL);
	ignore(write(1,LITLEN("Halting.\n")));
}

void check_init(struct event_base* eventbase, int sock) {

	gpgme_check_version(NULL);
	gpg_check(gpgme_new(&gpgme_ctx));
	gpgme_set_armor(gpgme_ctx, 1);
	gpg_check(gpgme_set_keylist_mode(
				  gpgme_ctx,
				  GPGME_KEYLIST_MODE_LOCAL));
#if 0
	gpg_check(gpgme_set_pinentry_mode(
				  gpgme_ctx,
				  GPGME_PINENTRY_MODE_ERROR));
	gpg_check(gpgme_set_locale(gpgme_ctx, ???, "UTF-8"));
#endif

	struct commit_later_data* data  = malloc(sizeof(struct commit_later_data));
	data->conn = bufferevent_socket_new(
		eventbase, sock, BEV_OPT_CLOSE_ON_FREE);
	data->eventbase = eventbase;
	ci.later = evtimer_new(eventbase, (void*)commit_later, data);

	activity_init(eventbase);
	hooks_init(eventbase, strlenstr(git_repository_path(repo)));

	struct evconnlistener* listener = evconnlistener_new(
		eventbase,
		on_accept, eventbase,
		LEV_OPT_CLOSE_ON_EXEC |
		LEV_OPT_CLOSE_ON_FREE |
		LEV_OPT_REUSEABLE |
		LEV_OPT_DEFERRED_ACCEPT,
		10,
		sock);
	evconnlistener_set_error_cb(listener, listen_error);
}
