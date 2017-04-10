#include "check.h"
#include <assert.h>
#include <stdlib.h> // malloc
#include <string.h> // memcpy
#include <stdio.h> // snprintf
#include <unistd.h> // execvp
#include <sys/mman.h> // mmap
#include <stdint.h> // uint*
#include <sys/wait.h> // waitpid
#include <ctype.h> // isspace

typedef uint16_t u16;
typedef int32_t i32;

static void waitfor(int pid) {
	assert(pid > 0);
	int status;
	assert(pid == waitpid(pid,&status,0));
	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);
}

struct check_context {
	uv_tcp_t stream;
	size_t checked; // parsed paths up to here
	size_t read; // chars read so far
	size_t space; // space in buffer
	char* buf;
};

typedef struct check_context *CC;

static void check_path(CC ctx, char* path, u16 len);

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
	puts("cleaned up");
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
	ctx->buf = NULL;
	ctx->space = ctx->read = ctx->checked = 0;
	uv_tcp_init(uv_default_loop(), &ctx->stream);
	uv_accept(server, (uv_stream_t*) &ctx->stream);
	uv_read_start((uv_stream_t*)ctx, alloc_cb, on_read);
}

static void maybe_commit(CC ctx, char* path, i32 lines, i32 words, i32 characters);

static void check_path(CC ctx, char* path, u16 len) {
	int pid = fork();
	if(pid == 0) {
		printf("adding %s\n",path);
		char* args[] = {
			"git","add",path, NULL
		};
		execvp("git",args);
	}
	waitfor(pid);

	char template[] = "derpXXXXXX";
	int io = mkstemp(template);
	unlink(template);
	pid = fork();
	if(pid == 0) {
		dup2(io,1);
		close(io);
		execlp("git","git","diff","HEAD","--word-diff=porcelain",NULL);
	}
	waitfor(pid);
	struct stat st;
	assert(0==fstat(io,&st));
	if(st.st_size == 0)
		return;
	char* diff = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, io, 0);
	assert(diff != MAP_FAILED);

	i32 characters = 0;
	i32 words = 0;
	i32 lines = 0;
	size_t i = 0;
	char found_diff = 0;
	for(;i<st.st_size;++i) {
		if(i < st.st_size - 2 &&
			 (i == 0 || diff[i] == '\n')) {
			if (i < st.st_size - 3 &&
					((diff[i+1] == '-' && diff[i+2] != '-') ||
					 (diff[i+1] == '+' && diff[i+2] != '+'))) {
				found_diff = 1;
				// we're at the newline before a -word or +word
				size_t j = i+2;
				char inword = 1;
				size_t lastw = j;
				for(;j<st.st_size;++j) {
					//printf("C: %c %d %d %d\n",diff[j],inword,lastw,j);
					if(isspace(diff[j])) {
						if(inword) {
							inword = 0;
							if(lastw + 1 < j)  {
								void commit(void) {
									/*
										printf("word: %d %d ",lastw,j);
									fwrite(diff+lastw,j-lastw,1,stdout);
									fputc('\n',stdout);
									*/
									++words;
								}
								if(lastw + 2 == j) {
									// 1 letter
									switch(diff[lastw]) {
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
						if(diff[j] == '\n')
							break;
					} else {
						if(!inword) {
							inword = 1;
							lastw = j;
						}
					}
					++characters;
				}
				i = j;
			} else if(diff[i+1] == '~') {
				// newlines in the source are represented by a \n~
				if(found_diff) {
					++lines;
					found_diff = 0;
				}
			}
		}
	}
DONE:
	printf("words %lu %lu %lu\n",lines, words, characters);
	maybe_commit(ctx, path, lines, words, characters);
}


static void commit_now(char* path, i32 lines, i32 words, i32 characters) {
	puts("committing");
	int pid = fork();
	if(pid == 0) {
		char message[0x1000];
		snprintf(message,0x1000,"auto (%s) %lu %lu %lu",
						 path, lines, words, characters);
		execlp("git","git","commit","-a","-m",message,NULL);
	}
	waitfor(pid);
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
} ci;

void check_init(void) {
	uv_timer_init(uv_default_loop(), &ci.committer);
	ci.next_commit = 0;
	ci.path = NULL;
	ci.words = ci.characters = 0;
}

static void commit_later(uv_timer_t* handle) {
	commit_now(ci.path, ci.lines, ci.words, ci.characters);
	ci.path = NULL; // just in case
}

static void maybe_commit(CC ctx, char* path, i32 lines, i32 words, i32 characters) {
	// see interpolate.py
	double d = (characters - 600) * (539 * characters - 32939)/5391.0;
	double test = (words - 50)*(2351*words - 23951)/294.0;
	if(test < d) d = test;
	test = (lines - 10)*(41*lines - 241)/3.0;
	if(test < d) d = test;

	// don't bother waiting if it's more than an hour
	if(d >= 3600) return;
	if(d <= 1) {
		uv_timer_stop(&ci.committer);
		commit_now(path,lines,words,characters);
	} else {
		time_t now = time(NULL);
		if(ci.next_commit == 0 || now + d < ci.next_commit) {
			// keep pushing the timer back, so we commit sooner if more changes
			printf("waiting %d\n",(int)d);
			uv_timer_stop(&ci.committer);
			ci.next_commit = now + d;
			ci.path = path;
			ci.words = words;
			ci.characters = characters;
			uv_timer_start((uv_timer_t*)&ci, commit_later, d * 1000, 0);
		}
	}
}
