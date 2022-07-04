/* Developed by Fabio Cinicolo */
#ifndef DAEMON
#define DAEMON

#define LOCKFILE "/var/run/keylogger-daemon.pid"

int daemonize(char * name);
int lockfile(int locked_file);
int daemonAlreadyRunning(int * locked_file);

#endif