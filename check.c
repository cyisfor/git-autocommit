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

	size_t words = 0;
	size_t i = 0;
	for(;i<info.st_size;++i) {
		if(i < info.st_size - 3 &&
			 (i == 0 || diff[i] == '\n') && 
			 ((diff[i+1] == '-' && diff[i+2] != '-') ||
				(diff[i+1] == '+' && diff[i+2] != '+'))) {
			// we're at the newline before a -word or +word
		}
	}
}
