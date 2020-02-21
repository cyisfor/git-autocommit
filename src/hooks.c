#define _GNU_SOURCE 			/* dlmopen */
#include "mystring.h"

#include "eventbase.h"
#include "ensure.h"
#include "repo.h"
#include "hooks.h"
#include "checkpid.h"

#include <sys/wait.h> // waitpid
#include <sys/mman.h> // mmap for shared semaphore

#include <semaphore.h>
#include <dl.h> // dlopen, dlsym
#include <unistd.h> // fork, exec*
#include <stdarg.h> // va_*
#include <stdio.h>
#include <stdbool.h>
#include <string.h> // memcmp
#include <limits.h> // PATH_MAX
#include <sys/stat.h>
#include <errno.h>

struct hook {
	string name;
	bool islib;
	union {
		struct {
			runner f;
			void* data;
		} run;
		char path[PATH_MAX];
	} u;
};

// tsearch is too opaque... can't debug problems!
// this is a tiny array anyway
static struct hook* hooks = NULL;
static size_t nhooks = 0;

static void load(const string location, const string name) {
		struct hook* hook = NULL;
	void init_hook(void) {
			hooks = realloc(hooks,sizeof(struct hook) * (nhooks+1));
			hook = hooks + nhooks;
			++nhooks;
			hook->name.base = malloc(nlen); // sigh
			memcpy(hook->name.base,name,nlen);
			hook->name.len = nlen;
			hook->islib = true; // eh
	}

	if(0 == stat(ZSTR(name),&sostat)) {
		init_hook();
		hook->islib = false;
		assert(realpath(name,hook->u.path));
		return;
	}

	bstring src = {};
	addstrn(&src, STRANDLEN(name));
	addstr(&src, ".c\0");

	if(0 != stat(ZSTR(src), &sostat)) {
		return;
		// no hook for this name exists
	}
	
	bstring dest = {};
	addstrn(&dest, STRANDLEN(name));
	addstr(&dest, ".so");

	void build_so() {
		// todo: reinitialize if the source changes...
		FILE* out = fopen(".temp.cmake","wt");
#define output_literal(lit) fwrite(LITLEN(lit), 1, out)
#define output_buf(buf, len) fwrite(buf, len, 1, out)
#include "make_module.cmake.snippet.c"
#undef output_buf
#undef output_literal
		fclose(out);
		ensure0(rename(".temp.cmake", "CMakeLists.txt"));
		int pid = fork();
		if(pid == 0) {
			execlp("cmake", "cmake", "-G", "Ninja", ".", NULL);
			abort();
		}
		int status = 0;
		waitpid(pid, &status, 0);
		ensure(WIFEXITED(status));
		ensure_eq(0, WEXITSTATUS(status));

		pid = fork();
		if(pid == 0) {
			execlp("ninja", "ninja", NULL);
			abort();
		}
		int status = 0;
		waitpid(pid, &status, 0);
		ensure(WIFEXITED(status));
		ensure_eq(0, WEXITSTATUS(status));
	}
	
	void load_so2(bool tried) {
		void* dll = dlmopen(LM_ID_NEWLM,
							so,
							RTLD_NOW | 
							RTLD_LOCAL);
		if(!dll) {
			if(tried) {
				fputs(stderr, dlerror());
				fputc(stderr, '\n');
				abort();
			}
			build_so();
			return load_so2(true);
		}

		typedef void* (*initter)(void);
		initter init = (initter) dlsym(dll,"init");
		init_hook();
		if(init) {
			hook->u.run.data = init();
		}
		hook->u.run.f = (runner) dlsym(dll,"run");
		if(hook->u.run.f == NULL) {
			fprintf(stderr, "your hook %.*s needs a run function.", (int)nlen, name);
		}
		hook->islib = true;
		return;
	}

	void load_so(void) {
		load_so2(false);
	}

	struct stat cstat, sostat;
	if(0 == stat(csource,&cstat)) {
		if(0 == stat(so,&sostat)) {
			if(sostat.st_mtime < cstat.st_mtime) {
				build_so();
			}
		} else {
			build_so();
		}
		return load_so();
	} else if(0 == stat(so,&sostat)) {
		return load_so();
	} else {
	}
	abort(); // nuever 
}

void hook_run(struct event_base* eventbase, const char* name, const size_t nlen, struct continuation after) {
	size_t i = 0;
	for(;i<nhooks;++i) {
		if(hooks[i].name.len == nlen &&
			 0==memcmp(hooks[i].name.base,name,nlen)) {
			break;
		}
	}
	if(i == nhooks) {
		// no hook
		continuation_run(after);
		return;
	}
	struct hook* hook = hooks+i;

	if(hook->islib) {
		assert(hook->u.run.f); 	/* if path is filled out this will be garbage though */
		hook->u.run.f(hook->u.run.data, after);
	} else {
		// the PID cannot be allowed to exit before we get our after handler in the list
		sem_t* ready;
		void* mem;
		if(after.func) {
			// ready must be ALLOCATED in shared memory, or it's not multiprocess
			// fuck sem_open
			mem = mmap(NULL,sizeof(sem_t),PROT_WRITE,MAP_ANONYMOUS|MAP_SHARED,-1,0);
			assert(mem != MAP_FAILED);
			ready = (sem_t*)mem;
			ensure0(sem_init(ready, 1, 0));
		}
		int pid = checkpid_fork();
		if(pid == 0) {
			if(after.func) {
				while(0 != sem_wait(ready)) {
					puts("ohpleaseohpleasedon'tdie");
					sleep(1);
				}
				puts("semaphore waited!");
				munmap(mem,sizeof(sem_t));
			}
			event_reinit(eventbase);
			char* args[] = { hook->u.path, NULL };
			execv(hook->u.path,args);
			if(errno == ENOEXEC || errno == EACCES) {
				perror("trying shell, since");
				printf("hook %s\n",hook->u.path);
				char* args[] = { "sh", hook->u.path, NULL };
				execv("/bin/sh",args);
				perror("nope");
			}
			perror(hook->u.path);
			abort();
		}
		
		checkpid(pid, "hook %s", name);
		if(after.func) {
			checkpid_after(pid, after);
			printf("ready to go after %d\n",pid);
			sem_post(ready);
			munmap(mem,sizeof(sem_t));
		}
		// this won't wait, so we can still do stuff
	}
}

void hooks_init(struct event_base* eventbase) {
	assert0(chdir(git_repository_path(repo)));
	mkdir("hooks",0755);
	assert0(chdir("hooks"));
	setenv("LD_LIBRARY_PATH",".",1);
	char buf[PATH_MAX];
	load(LITLEN("pre-commit"));
	load(LITLEN("post-commit"));
	assert0(chdir(git_repository_workdir(repo)));
	checkpid_init(eventbase);
}
