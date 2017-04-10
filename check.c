struct check_context {
	size_t checked; // parsed paths up to here
	size_t read; // chars read so far
	size_t space; // space in buffer
	char* buf;
};

void check_consume(CC ctx, const char* buf, size_t n) {
	if(ctx->read + n > ctx->space) {
		ctx->space = ((ctx->read + n) / BLOCKSIZE + 1) * BLOCKSIZE;
		ctx->buf = realloc(ctx->buf, ctx->space);
	}
	memcpy(ctx->buf + ctx->read, buf, n);

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
		check_path(path,size);
		ctx->checked += 2 + size;
	}
}

void check_path(char* path, u16 len) {
	char* args[] = {
		"git","add",path, NULL
	};
	call(args);
	char* template = "derpXXXXXX";
	int io = mkstemp(template);
	unlink(template);
	int pid = fork();
	if(pid == 0) {
		dup2(io,1);
		close(io);
		execlp("git","git","diff","HEAD",
					 "--word-diff=porcelain",NULL);
	}
	waitfor(pid);
	struct stat info;
	assert(fstat(io,&info));
	char* diff = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, io, 0);
	assert(diff != MAP_FAILED);

	size_t characters = 0;
	size_t words = 0;
	size_t i = 0;
	for(;i<info.st_size;++i) {
		if(i < info.st_size - 3 &&
			 (i == 0 || diff[i] == '\n') && 
			 ((diff[i+1] == '-' && diff[i+2] != '-') ||
				(diff[i+1] == '+' && diff[i+2] != '+'))) {
			// we're at the newline before a -word or +word
			++words;
			size_t j = i+3;
			for(;;) {
				if(j >= info.st_size) {
					goto DONE;
				}
				if(diff[j] == '\n') break;
				++characters;
			}
			i = j;
		}
	}
DONE:
	maybe_commit(words, characters);
}

time_t next_commit = 0;
uv_timer_t committer;

void check_init(void) {
	uv_timer_init(uv_default_loop(),&committer);
}

void commit_now(uv_timer_t* handle) {
	int pid = fork();
	if(pid == 0) {
		char message[0x1000];
		snprintf(message,
		execlp("git","commit","-a","-m",message,NULL);


void maybe_commit(size_t words, size_t characters) {
	// 60 characters = 1min to commit (60s), 600 characters means commit now.
	// m = (60 - 0) / (60 - 600) = - 60 / 540
	// d = m * c + b, 0 = m * 60 + b, b = -m * 60 = 3600 / 540 = 60 / 9
	// d = 60 * (1/9 - c / 540)
	double delay1 = 60 * (1.0 / 9 - characters / 540);
	// 10 words = 1min to commit, 50 words = commit now
	// m = (60 - 0) / (10 - 50) = -60 / 40 = -3/2
	// b = -m * 60 = 3/2 * 60 = 3 * 30 = 90
	// d = 90 - 3 * w / 2
	double delay2 = 90 - 3 * words / 2.0;
	double d = delay1;
	if(delay1 < delay2) d = delay2;

	// don't bother waiting if it's more than an hour
	if(d > 3600) return;

	if(d <= 1) {
		uv_timer_stop(&committer);
		commit_now();
	} else {
		time_t now = time(NULL);
		if(now + d > next_commit) {
			uv_timer_stop(&committer);
			next_commit = now + d;
			uv_timer_start(&committer, commit_now, d * 1000, 0);
		}
	}
}
