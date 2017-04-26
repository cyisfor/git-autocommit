#include <stdlib.h>

void net_set_addr(void);
int net_bind(void);
int net_connect(void);
pid_t net_pid(int sock);
void net_forkhack(pid_t pid);
