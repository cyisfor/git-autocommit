#include "repo.h"
#include "check.h"

int main(int argc, char *argv[])
{
	repo_init(".");
	check_init();
	check_path(NULL,"check.c",7);
	return 0;
}
