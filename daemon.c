#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "daemon.h"

int daemonize()
{
    pid_t pid;
    struct rlimit limit;

    umask(0); /* Resetting process file mode creation mask */

    if (getrlimit(RLIMIT_NOFILE, &limit) < 0) /* Getting max number of file descriptors that process can open. */
        return 0;

    pid = fork(); /* Parent exits, Child inherits session and process group id of the terminated parent. Child is now orphane and is attached to init process. */
    if (pid < 0)
        return 0;
    if (pid)
        exit(EXIT_SUCCESS);

    setsid(); /* A new session is created. Child becomes leader of the session and of a new process group, it is detached from the parent's controlling terminal */

    pid = fork(); /* After the first fork, the process could still be take control of a TTY because it is the session leader!.
                    This fork will generate a new child which will not be the session leader anymore. We have successfully daemonized our process. */
    if (pid < 0)
        return 0;
    if (pid)
        exit(EXIT_SUCCESS);

    /* chdir("/");  Changes the working directory to a safe one. */

    if (limit.rlim_max == RLIM_INFINITY)
        limit.rlim_max = 1024;

    for (long unsigned int i = 0; i < limit.rlim_max; i++) /* Closing all file descriptors */
        close(i);

    return 1;
}

int lockfile(int fd) /* Simple lock file */
{
    struct flock fl;
    fl.l_type = F_WRLCK; /* Write lock */
    fl.l_start = 0;      /* Starting to lock from the beginning */
    fl.l_whence = SEEK_SET;
    fl.l_len = 0; /* Locking the entire file  */
    return (fcntl(fd, F_SETLK, &fl));
}

int daemonAlreadyRunning(int *lock_file)
{
    char buf[16];
    int fd = open(LOCKFILE, O_RDWR | O_CREAT, LOCKMODE); /* Opening file */
    if (fd < 0)                                          /* If open went wrong */
        return 2;
    if (lockfile(fd) < 0) /* If lockfile went wrong */
    {
        if (errno == EACCES || errno == EAGAIN) /* If lockfile failed with errno set to EACCESS or EAGAIN
                                                   it means that the lock has already been running so another is  daemon is running */
        {
            close(fd);
            return 1;
        }
        close(fd);
        return 2;
    }
    /* If Daemon acquired write lock, writes its pid into the locked file */
    ftruncate(fd, 0);
    sprintf(buf, "%ld", (long)getpid());
    write(fd, buf, strlen(buf) + 1);
    *lock_file = fd;
    return 0;
}
