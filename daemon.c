/* Developed by Fabio Cinicolo */

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "daemon.h"

int daemonize()
{
    pid_t pid;
    struct rlimit limit;

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

    chdir("/"); // Changes the working directory to a safe one.

    if (limit.rlim_max == RLIM_INFINITY)
        limit.rlim_max = 1024;

    for (int i = 0; i < limit.rlim_max; i++) // Closing all file descriptors
        close(i);

    return 0;
}

// struct sigaction sa;

// sa.sa_handler = &handleSIGCHLD;
// sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
// sigaction(SIGCHLD, &sa, NULL);

// pid_t child;

// sigset_t set;

// sigprocmask(0, NULL, &set);

// if (sigismember(&set, SIGCHLD))
//     printf("sigchld\n");

// for (int i = 0; i < 10; i++)
// {
//     child = fork();
//     if (!child)
//     {
//         usleep(1000000 % rand());
//         exit(i);
//     }
// }
// while (1);

// void handleSIGCHLD(int signum)
// {

//     sigset_t set;

//     sigprocmask(0, NULL, &set);

//     if (sigismember(&set, SIGCHLD))
//         printf("sigchld in sigchld\n");

//     printf("ciao\n");
//     int main_errno = errno;
//     int status;
//     pid_t child;

//     while ((child = waitpid(-1, &status, WNOHANG)) > 0)
//         printf("Child %d terminated with status: %d\n", child, WEXITSTATUS(status));

//     errno = main_errno;

// }