enum operations { OP_ADD, OP_QUIT, OP_INFO, OP_FORCE };

#include <unistd.h> // pid_t
#include <time.h> // time_t


struct info_message {
	pid_t pid;
	size_t lines;
	size_t words;
	size_t characters;
	time_t next_commit;
};
