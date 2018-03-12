enum operations { ADD, QUIT, INFO, FORCE };

#include <unistd.h> // pid_t
#include <time.h> // time_t


struct info_message {
	pid_t pid;
	size_t lines;
	size_t words;
	size_t characters;
	time_t next_commit;
};
