#include "network-ipc.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

int openConnectionWithServer(char *ip, short port)
{
    int sock_fd;
    struct hostent *host_info;
    struct sockaddr_in server;

    if ((host_info = gethostbyname(ip)) == NULL)
        return -1;

    sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_fd < 0)
        return -1;

    bzero(&server, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr = *((struct in_addr *)host_info->h_addr);
    server.sin_port = htons(port);

    if (connect(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
        return -1;

    return sock_fd;
}
