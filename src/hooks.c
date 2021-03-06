#ifndef _GNU_SOURCE
#define _GNU_SOURCE				/* dlmopen */
#endif

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
		bstring path;
	} u;
};

// tsearch is too opaque... can't debug problems!
// this is a tiny array anyway

/* be SURE to zero this */
struct module {
	string name;
	bstring src;
	bstring dest;
	struct stat srcstat;
	struct stat deststat;
	bool islib;
	bstring path;
};

struct modules {
	struct module* D;
	size_t len;
};	

struct hooks {
	struct hook* D;
	size_t len;
} hooks = {};

static void load(const string location, struct modules modules, const string project) {
	if(modules.len == 0) return;
	assert(hooks.D == NULL);
	/* first, set up the variables, clean out the nonexistent modules */
	int i;
	int curmod = 0;
	for(i=0;i<modules.len;++i) {
		struct module* mod = modules.D + i;
		const char* zname = ZSTR(mod->name);
		if(0 == stat(zname,&mod->deststat)) {
			mod->islib = false;
			ensure0(realpath(zname,(char*)strreserve(&mod->path, PATH_MAX)));
			mod->path.len = strlen((const char*)mod->path.base);
			*(strreserve(&mod->path, 1)) = '\0';
		} else {
			straddstr(&mod->src, mod->name);
			stradd(&mod->src, ".c");
			*(strreserve(&mod->src, 1)) = '\0';
			if(0 != stat(CHARSTR(mod->src), &mod->srcstat)) {
				// no hook for this name exists
				continue;
			}
			straddstr(&mod->dest, mod->name);
			stradd(&mod->dest, ".so");
			*(strreserve(&mod->dest, 1)) = '\0';

			mod->islib = true;
		}	
		if(curmod  != i) {
			modules.D[curmod] = *mod;
		}
		++curmod;
	}
	ZSTR_done();
	modules.len = curmod;
	if(modules.len == 0) return;

	void build_modules(void) {
		// todo: reinitialize if the source changes...
		FILE* out = fopen(".temp.cmake","at");
#define output_literal(lit) fwrite(LITLEN(lit), 1, out)
#define output_buf(buf, len) fwrite(buf, len, 1, out)
#include "make_module.cmake.snippet.c"
#undef output_buf
#undef output_literal
		fclose(out);
		ensure0(rename(".temp.cmake", "CMakeLists.txt"));
		int pid = fork();
		if(pid == 0) {
			mkdir("build", 0755);
			ensure0(chdir("build"));
			execlp("cmake", "cmake", "-G", "Ninja", "..", NULL);
			abort();
		}
		int status = 0;
		waitpid(pid, &status, 0);
		ensure(WIFEXITED(status));
		ensure_eq(0, WEXITSTATUS(status));

		pid = fork();
		if(pid == 0) {
			execlp("ninja", "ninja", "-C", "build", "install", NULL);
			abort();
		}
		status = 0;
		waitpid(pid, &status, 0);
		ensure(WIFEXITED(status));
		ensure_eq(0, WEXITSTATUS(status));
	}
	/* should we build every time, or only when they fail to load?
	   If the source updates, and the module's compiled, this needs to run.
	   So either we check the mtimes ourselves or...
	   build_modules();
	*/
	for(i=0;i<modules.len;++i) {
		struct module* mod = modules.D + i;
		if(!mod->islib) continue;
		if(0 != stat(CHARSTR(mod->dest), &mod->deststat)) {
			continue;
		}
		if(mod->srcstat.st_mtime < mod->deststat.st_mtime) continue;
		if(mod->srcstat.st_mtime == mod->deststat.st_mtime &&
		   mod->srcstat.st_mtim.tv_nsec < mod->deststat.st_mtim.tv_nsec) continue;
		build_modules();
		break;
	}

	hooks.D = calloc(1, sizeof(struct hook)*modules.len);
	hooks.len = 0;
	struct hook* next_hook(void) {
		return hooks.D + (++hooks.len) - 1;
	}
	struct hook* try_load_so(const struct module* mod, const bool tried) {
		void* dll = dlopen(
							ZSTR(STRING(mod->dest)),
							RTLD_NOW | 
							RTLD_LOCAL);
		if(!dll) {
			if(tried) {
				fputs(dlerror(), stderr);
				fputc('\n', stderr);
				abort();
			}
			build_modules();
			return try_load_so(mod, true);
		}

		typedef void* (*initter)(void);
		initter init = (initter) dlsym(dll,"init");
		struct hook* hook = next_hook();
		if(init) {
			hook->u.run.data = init();
		}
		hook->u.run.f = (runner) dlsym(dll,"run");
		if(hook->u.run.f == NULL) {
			fprintf(stderr, "your hook %.*s needs a run function.",
					STRING_FOR_PRINTF(mod->name));
		}
		return hook;
	}

	void load_mod(struct module* mod) {
		struct hook* hook;
		if(mod->islib) {
			hook = try_load_so(mod, false);
			hook->islib = true;
		} else {
			hook = next_hook();
			hook->u.path = mod->path;
			mod->path = (bstring){};
			hook->islib = false;
		}
		hook->name = mod->name;
	}

	for(i=0;i<modules.len;++i) {
		load_mod(modules.D + i);
	}

	hooks.D = realloc(hooks.D, sizeof(*hooks.D)*hooks.len);
	
	for(i=0;i<modules.len;++i) {
		strclear(&modules.D[i].src);
		strclear(&modules.D[i].dest);
	}
}

void hook_run(struct event_base* eventbase, const string name, struct continuation after) {
	size_t i = 0;
	for(;i<hooks.len;++i) {
		if(hooks.D[i].name.len == name.len &&
			 0==memcmp(hooks.D[i].name.base,name.base,name.len)) {
			break;
		}
	}
	if(i == hooks.len) {
		// no hook
		continuation_run(after);
		return;
	}
	struct hook* hook = hooks.D+i;

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
			char* args[] = { (char*)hook->u.path.base, NULL };
			execv((char*)hook->u.path.base,args);
			if(errno == ENOEXEC || errno == EACCES) {
				perror("trying shell, since");
				printf("hook %.*s\n",STRING_FOR_PRINTF(hook->u.path));
				char* args[] = { "sh", (char*)hook->u.path.base, NULL };
				execv("/bin/sh",args);
				perror("nope");
			}
			record(ERROR, "%.*s", STRING_FOR_PRINTF(hook->u.path));
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

void hooks_init(struct event_base* eventbase, const string project) {
	ensure0(chdir(git_repository_path(repo)));
	mkdir("hooks",0755);
	ensure0(chdir("hooks"));
	bstring location = {};
	/* note: it's useless to set LD_LIBRARY_PATH here, and ld.so has no other way for us to do
	   so other than HURR DURR IM GOOD PROGRAMMER I MAKE THE BYTES */
	location.len = strlen(getcwd((char*)strreserve(&location, PATH_MAX), PATH_MAX));
	*strreserve(&location, 1) = '\0';
	struct module modulestore[] = {
#define ONE(lit) { .name = LITSTR(lit) }
		ONE("pre-commit"),
		ONE("post-commit")
#undef ONE
	};
	struct modules modules = {
		.D = modulestore,
		.len = sizeof(modulestore)/sizeof(*modulestore)
	};
	load(STRING(location), modules, project);

	strclear(&location);
	ensure0(chdir(git_repository_workdir(repo)));
	checkpid_init(eventbase);
}
