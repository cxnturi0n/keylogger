/* Developed by Fabio Cinicolo */

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include "daemon.h"

int daemonize(char *name)
{
    pid_t pid;
    struct rlimit limit;

    umask(0); // Resetting process file mode creation mask

    if (getrlimit(RLIMIT_NOFILE, &limit) < 0) // Getting max number of file descriptors that process can open.
        return -1;

    pid = fork(); // Parent exits, Child inherits session and process group id of the terminated parent. Child is now orphane and is attached to init process.
    if (pid < 0)
        return -1;
    if (pid)
        exit(EXIT_SUCCESS);

    setsid(); // A new session is created. Child becomes leader of the session and of a new process group, it is detached from the parent's controlling terminal (TTY).

    pid = fork(); // After the first fork, the process could still be take control of a TTY because it is the session leader!.
                  //  This fork will generate a new child which will not be the session leader anymore. We have successfully daemonized our process.
    if (pid < 0)
        return -1;
    if (pid)
        exit(EXIT_SUCCESS);

    openlog(name, LOG_PID, LOG_USER); // Pid and name will appear in the syslog

    // chdir("/"); // Changes the working directory to a safe one.

    if (limit.rlim_max == RLIM_INFINITY)
        limit.rlim_max = 1024;

    for (int i = 0; i < limit.rlim_max; i++) // Closing all file descriptors
        close(i);

    return 0;
}

int lockfile(int fd) // Simple lock file
{
    struct flock fl;
    fl.l_type = F_WRLCK; // Write lock
    fl.l_start = 0;      // Starting to lock from the beginning
    fl.l_whence = SEEK_SET;
    fl.l_len = 0; // Locking the entire file
    return (fcntl(fd, F_SETLK, &fl));
}

int daemonAlreadyRunning(int *lock_file)
{
    char buf[16];
    int fd = open(LOCKFILE, O_RDWR | O_CREAT, LOCKMODE); // Opening lock file
    if (fd < 0)                                          // If open went wrong
        syslog(LOG_ERR, "Couldn't open %s: %s", LOCKFILE, strerror(errno)), exit(EXIT_FAILURE);
    if (lockfile(fd) < 0) // If lockfile went wrong
    {
        if (errno == EACCES || errno == EAGAIN) // If lockfile failed with errno set to EACCESS or EAGAIN it means that the lock has already been running so another is  daemon is running
        {
            syslog(LOG_ERR, "Another copy of this daemon is running! Quitting..");
            close(fd);
            return 1;
        }
        syslog(LOG_ERR, "Can’t lock %s: %s", LOCKFILE, strerror(errno)), exit(EXIT_FAILURE);
    }
    // If Daemon acquired lock, writes its pid into the lock file
    syslog(LOG_INFO, "Daemon successfully acquired lock, pid can be read into: %s", LOCKFILE);
    ftruncate(fd, 0);
    sprintf(buf, "%ld", (long)getpid());
    write(fd, buf, strlen(buf) + 1);
    *lock_file = fd;
    return 0;
}