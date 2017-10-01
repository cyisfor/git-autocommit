#include "ensure.h"
#include "repo.h"
#include "hooks.h"
#include "myassert.h"
#include "checkpid.h"

#include <sys/mman.h> // mmap

#include <dlfcn.h> // dlopen, dlsym
#include <unistd.h> // fork, exec*
#include <stdarg.h> // va_*
#include <stdio.h>
#include <stdbool.h>
#include <string.h> // memcmp
#include <limits.h> // PATH_MAX
#include <sys/stat.h>
#include <error.h>
#include <errno.h>

#define LITLEN(s) s,sizeof(s)-1

typedef struct buf {
	char* base;
	int len;
} buf;

static int compare(struct buf* a, struct buf* b) {
	int len = a->len;
	if(len != b->len)
		return len - b->len;
	return memcmp(a->base, b->base, len);
}

struct hook {
	buf name;
	bool islib;
	union {
		struct {
			runner f;
			void* data;
		} run;
		char path[PATH_MAX];
	} u;
};

#ifndef PLUGIN_FLAGS
#define PLUGIN_FLAGS "-g", "-O2",
#endif

// tsearch is too opaque... can't debug problems!
// this is a tiny array anyway
static struct hook* hooks = NULL;
size_t nhooks = 0;

static void load(const char* name, size_t nlen) {
	char csource[0x100];
	memcpy(csource,name,nlen);
	size_t len = nlen;
	csource[nlen] = '.';
	csource[nlen+1] = 'c';
	csource[nlen+2] = '\0';

	char so[0x100];
	memcpy(so+2,name,nlen);
	so[0] = '.';
	so[1] = '/';
	so[nlen+2] = '.';
	so[nlen+3] = 's';
	so[nlen+4] = 'o';
	so[nlen+5] = '\0';

	// todo: reinitialize if the source changes...

	void build_so() {
		int pid = fork();
		if(pid == 0) {
			char* cc = getenv("CC");
			if(cc == NULL) cc = "cc";
			char* args[] = {
				cc,
				PLUGIN_FLAGS
				"-fPIC",
				"-shared",
				"-I", SOURCE_LOCATION, // -D this
				"-o",
				so,
				csource,
				NULL
			};
			execvp(cc,args);
		}
		checkpid(pid, "gcc %s",name);
	}

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

	void load_so() {
		void* dll = dlopen(so,RTLD_LAZY | RTLD_LOCAL);
		assert(dll);
		const char* e = dlerror();
		if(e != NULL) {
			error(23,0,e);
			abort();
		}
		typedef void* (*initter)(void);
		initter init = (initter) dlsym(dll,"init");
		init_hook();
		if(init) {
			hook->u.run.data = init();
		}
		hook->u.run.f = (runner) dlsym(dll,"run");
		hook->islib = true;
		return;
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
		if(0 == stat(name,&sostat)) {
			init_hook();
			hook->islib = false;
			assert(realpath(name,hook->u.path));
			return;
		} else {
			return;
			// no hook for this name exists
		}
	}
	abort(); // nuever 
}

void hook_run(const char* name, const size_t nlen, uv_async_t* after) {
	size_t i = 0;
	for(;i<nhooks;++i) {
		if(hooks[i].name.len == nlen &&
			 0==memcmp(hooks[i].name.base,name,nlen)) {
			break;
		}
	}
	if(i == nhooks) {
		// no hook
		if(after) uv_async_send(after);
		return;
	}
	struct hook* hook = hooks+i;

	if(hook->islib) {
		hook->u.run.f(hook->u.run.data, after);
	} else {
		// the PID cannot be allowed to exit before we get our after handler in the list
		sem_t* ready;
		void* mem;
		if(after) {
			// ready must be ALLOCATED in shared memory, or it's not multiprocess
			// fuck sem_open
			mem = mmap(NULL,sizeof(sem_t),PROT_WRITE,MAP_ANONYMOUS|MAP_SHARED,-1,0);
			assert(mem != MAP_FAILED);
			ready = (sem_t*)mem;
			ensure0(sem_init(ready, 1, 0));
		}
		int pid = checkpid_fork();
		if(pid == 0) {
			if(after) {
				while(0 != sem_wait(ready)) {
					puts("ohpleaseohpleasedon'tdie");
					sleep(1);
				}
				puts("semaphore waited!");
				munmap(mem,sizeof(sem_t));
			}
			char* args[] = { hook->u.path, NULL };
			execv(hook->u.path,args);
			if(errno == ENOEXEC || errno == EACCES) {
				perror("trying shell");
				char* args[] = { "sh", hook->u.path, NULL };
				execv("/bin/sh",args);
			}
			perror(hook->u.path);
			abort();
		}
		
		checkpid(pid, "hook %s", name);
		if(after) {
			checkpid_after(pid, after);
			sem_post(ready);
			munmap(mem,sizeof(sem_t));
		}
		// this won't wait, so we can still do stuff
	}
}

void hooks_init(void) {
	assert0(chdir(git_repository_path(repo)));
	mkdir("hooks",0755);
	assert0(chdir("hooks"));
	char buf[PATH_MAX];
	load(LITLEN("pre-commit"));
	load(LITLEN("post-commit"));
	assert0(chdir(git_repository_workdir(repo)));
	checkpid_init();
}