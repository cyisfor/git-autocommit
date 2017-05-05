#include "repo.h"
#include "hooks.h"
#include "myassert.h"

#include <search.h> // tfind, tsearch
#include <dlfcn.h> // dlopen, dlsym
#include <unistd.h> // fork, exec*
#include <stdarg.h> // va_*
#include <stdio.h>
#include <sys/wait.h> // waitpid
#include <stdbool.h>
#include <string.h> // memcmp
#include <limits.h> // PATH_MAX
#include <sys/stat.h>
#include <error.h>
#include <errno.h>


#define LITLEN(s) s,sizeof(s)-1

static void checkpid(int pid, char* fmt, ...) {
	va_list arg;
	va_start(arg, fmt);
	void erra(const char* fmt2, ...) {
		vfprintf(stderr,fmt, arg);
		va_end(arg);
		va_start(arg, fmt2);
		fputc(' ',stderr);
		vfprintf(stderr,fmt2,arg);
		fputc('\n',stderr);
		abort();
	}
	if(pid < 0) {
		erra("bad pid %d",pid);
	}
	int status;
	if(-1 == waitpid(pid, &status, 0)) {
		error(errno,errno,"couldn't wait?");
	}
	if(WIFSIGNALED(status)) {
		erra("died with signal %d",WTERMSIG(status));
	} else if(WIFEXITED(status)) {
		int res = WEXITSTATUS(status);
		if(res != 0) {
			erra("exited with %d",res);
		}
	}
}

typedef void (*runner)(void*);

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

static struct hook* new_hook(const char* name, size_t nlen) {
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
		checkpid(pid, "gcc died building %s",name);
	}
	struct hook* hook = NULL;

	void init_hook(void) {
			hook = malloc(sizeof(struct hook));
			hook->name.base = malloc(nlen); // sigh
			memcpy(hook->name.base,name,nlen);
			hook->name.len = nlen;
	}

	struct hook* load_so() {
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
		return hook;
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
			return hook;
		} else {
			return NULL;
			// no hook for this name exists
		}
	}
	abort(); // nuever 
}

static void* hooks = NULL;

void hook_run(const char* name, const size_t nlen) {
	const struct hook search = {
		name: { name, nlen }
	};
	struct hook* hook = *((struct hook**)tfind(&search, &hooks, (void*)compare));
	if(!hook) {
		return;
	}

	if(hook->islib) {
		hook->u.run.f(hook->u.run.data);
	} else {
		int pid = fork();
		if(pid == 0) {
			char* args[] = { hook->u.path };
			execv(hook->u.path,args);
			abort();
		}
		assert_gt(0,pid);
		int status;
		if(-1 == waitpid(pid, &status, 0)) {
			error(errno,errno,"couldn't wait?");
		}
		if(WIFSIGNALED(status)) {
			error(WTERMSIG(status),0,"%s hook died with signal %d",name,WTERMSIG(status));
		} else if(WIFEXITED(status)) {
			int res = WEXITSTATUS(status);
			if(res != 0) {
				error(res,0,"%s hook exited with %d",name,res);
			}
		}
	}
}

static void load(const char* name, const size_t nlen) {
	struct hook* hook = new_hook(name,nlen);
	if(hook) 
		tsearch((void*) hook, &hooks, (void*)compare);
}

void hooks_init(void) {
	assert0(chdir(git_repository_path(repo)));
	mkdir("hooks",0755);
	assert0(chdir("hooks"));
	char buf[PATH_MAX];
	load(LITLEN("pre-commit"));
	load(LITLEN("post-commit"));
	assert0(chdir(git_repository_workdir(repo)));
}
