#ifndef IO_NETWORKING
#define IO_NETWORKING
#include "keyboard-event.h"
#include <stdlib.h>


int sendEventsToServer(int server, event *events, size_t size);
int openConnectionWithServer(char*ip, short port);

#endif