#ifndef NETWORK_IPC
#define NETWORK_IPC
#include <stdlib.h>
#include <linux/input.h>

int openConnectionWithServer(char*ip, short port);

#endif