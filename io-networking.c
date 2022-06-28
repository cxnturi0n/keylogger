#include "io-networking.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

int sendEventsToServer(int server, event *events, size_t to_write)
{
    size_t written;
    do
    {
        written = send(server, events, to_write, 0);
        if (written < 0)
        {
            return -1;
            // SAVE INTO DAEMON LOG FILE
        }
        events += written;
        to_write -= written;
    } while (to_write > 0);
    char useless;
    if (!recv(server, &useless, 1, MSG_DONTWAIT))
    {
        return 1;
        // PRINT TO LOG THAT DAEMON TERMINATED BECAUSE SERVER CLOSED CONNECTION
        exit(EXIT_SUCCESS);
    }
    return 0;
}

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
