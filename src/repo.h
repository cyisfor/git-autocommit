// holy fuuuuuuuuuck
#include <git2/repository.h>
#include <git2/errors.h> 

extern git_repository* repo;
int repo_discover_init(char* start, int len);
int repo_init(const char* start);
size_t repo_relative(char** path, size_t plen);
void repo_check(git_error_code);
void repo_add(const char* path);
