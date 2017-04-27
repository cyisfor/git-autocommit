#include "repo.h"

#include <search.h> // tfind, tsearch
#include <dlfcn.h> // dlopen, dlsym
#include <unistd.h> // fork, exec*
#include <stdarg.h> // va_*
#include <stdio.h>
#include <sys/wait.h> // waitpid

#define LITLEN(s) s,sizeof(s)-1

static void checkpid(int pid, char* fmt, ...) {
	va_list arg;
	va_start(fmt, arg);
	void erra(const char* fmt2, ...) {
		vfprintf(stderr,arg);
		va_end(arg);
		va_start(fmt2,arg);
		fputc(' ',stderr);
		vfprintf(stderr,arg);
		fputc('\n',stderr);
		abort();
	}
	if(pid < 0) {
		erra("bad pid %d",pid);
	}
	int status;
	assert_ne(-1,waitpid(pid, &status, 0));
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

struct hook {
	uv_buf_t name;
	bool islib;
	union {
		struct {
			runner f;
			void* data;
		} run;
		char path[PATH_MAX];
	} u;
};

static bool initialize_hook(struct hook* hook, const char* name, size_t nlen) {
	hook->name = { name, nlen };

	char csource[0x100];
	memcpy(csource,name,nlen);
	size_t len = nlen;
	csource[nlen] = '.';
	csource[nlen+1] = 'c';
	csource[nlen+2] = '\0';

	char so[0x100];
	memcpy(so,name,nlen);
	so[nlen] = '.';
	so[nlen+1] = 's';
	so[nlen+2] = 'o';
	so[nlen+3] = '\0';

	// todo: reinitialize if the source changes...

	void build_so() {
		int pid = fork();
		if(pid == 0) {
			char* cc = getenv("CC");
			if(cc == NULL) cc = "cc";
			char* args[] = {
				cc,
				"-shared",
				"-o",
				so,
				csource,
				NULL
			};
			execvp(cc,args);
		}
		checkpid(pid, "gcc died building %s",name);
	}

	void load_so() {
		void* dll = dlopen(so,RTLD_LAZY | RTLD_LOCAL);
		assert(dll);
		typedef void* (*initter)(void);
		initter init = (initter) dlsym(dll,"init");
		if(init) {
			hook->run.data = init();
		}
		hook->run.f = (runner) dlsym(dll,"run");
		hook->islib = true;
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
			hook->islib = false;
			assert(realpath(name,hook->u.path));
		} else {
			return false;
			// no hook for this name exists
		}
	}
	return true;
}

static void* hooks = NULL;

static void run(const char* name, const size_t nlen) {
	struct hook search = {
		name: { name, nlen }
	};
	struct hook* hook = tfind(&search, &hooks, compare);
	if(!hook) {
		return;
	}

	if(hook->islib) {
		hook->u.run.f(hook->u.run.data);
	} else {
		int pid = fork();
		if(pid == 0) {
			char* args[] = { hook->u.path };			
			execv(path,args);
			abort();
		}
		assert_gt(0,pid);
		int status;
		assert_ne(-1,waitpid(pid, &status, 0));
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

void hooks_run(void) {
	run(LITLEN("pre-commit"));
	run(LITLEN("post-commit"));
}

static void load(const char* name, const size_t nlen) {
	struct hook* hook = malloc(sizeof(struct hook));
	initialize_hook(hook,name,nlen);
	assert0(tsearch((void*) hook, &hooks, compare));
}

void hooks_init(void) {
	assert0(chdir(git_repository_path(repo)));
	mkdir("hooks",0755);
	assert0(chdir("hooks"));
	load(LITLEN("pre-commit"));
	load(LITLEN("post-commit"));
	assert0(chdir(git_repository_workdir(repo)));
}
