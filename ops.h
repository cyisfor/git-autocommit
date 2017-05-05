enum operations { ADD, QUIT, INFO };

#include <unistd.h> // pid_t



struct info_message {
	pid_t pid;
	size_t lines;
	size_t characters;
	size_t words;
	time_t next_commit;
}
