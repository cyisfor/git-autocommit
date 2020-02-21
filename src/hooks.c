#define _GNU_SOURCE 			/* dlmopen */
#include "mystring.h"
#include "ensure.h"
#include "config_locations.h"

#include "eventbase.h"
#include "ensure.h"
#include "repo.h"
#include "hooks.h"
#include "checkpid.h"

#include <sys/wait.h> // waitpid
#include <sys/mman.h> // mmap for shared semaphore

#include <semaphore.h>
#include <dlfcn.h> // dlopen, dlsym
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
			hook->name = name;
			hook->islib = true; // eh
	}
	struct stat cstat, sostat;

	const char* zname = ZSTR(name);
	if(0 == stat(zname,&sostat)) {
		init_hook();
		hook->islib = false;
		assert(realpath(zname,hook->u.path));
		return;
	}
	zname = NULL;

	bstring src = {};
	straddn(&src, STRANDLEN(name));
	stradd(&src, ".c\0");

	if(0 != stat(src.base, &cstat)) {
		return;
		// no hook for this name exists
	}
	--src.len; 					/* no \0 in our CMakeLists.txt plz */
	ZSTR_done();
	bstring dest = {};
	straddn(&dest, STRANDLEN(name));
	stradd(&dest, ".so");

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
			execlp("ninja", "ninja", "install", NULL);
			abort();
		}
		status = 0;
		waitpid(pid, &status, 0);
		ensure(WIFEXITED(status));
		ensure_eq(0, WEXITSTATUS(status));
	}
	
	void load_so2(bool tried) {
		void* dll = dlmopen(LM_ID_NEWLM,
							ZSTR(STRING(dest)),
							RTLD_NOW | 
							RTLD_LOCAL);
		if(!dll) {
			if(tried) {
				fputs(dlerror(), stderr);
				fputc('\n', stderr);
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
			fprintf(stderr, "your hook %.*s needs a run function.",
					STRING_FOR_PRINTF(name));
		}
		hook->islib = true;
		return;
	}

	void load_so(void) {
		load_so2(false);
	}

	return load_so();
}

void hook_run(struct event_base* eventbase, const string name, struct continuation after) {
	size_t i = 0;
	for(;i<nhooks;++i) {
		if(hooks[i].name.len == name.len &&
			 0==memcmp(hooks[i].name.base,name.base,name.len)) {
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
	ensure0(chdir(git_repository_path(repo)));
	mkdir("hooks",0755);
	ensure0(chdir("hooks"));
	bstring location = {};
	/* note: it's useless to set LD_LIBRARY_PATH here, and ld.so has no other way for us to do
	   so other than HURR DURR IM GOOD PROGRAMMER I MAKE THE BYTES */
	location.len = strlen(getcwd(strreserve(&location, PATH_MAX), PATH_MAX));
	*strreserve(&location, 1) = '\0';
	load(STRING(location), LITSTR("pre-commit"));
	load(STRING(location), LITSTR("post-commit"));
	strclear(&location);
	ensure0(chdir(git_repository_workdir(repo)));
	checkpid_init(eventbase);
}
