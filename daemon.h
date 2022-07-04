#ifndef DAEMON
#define DAEMON

#define LOCKFILE "/var/run/keylogger-daemon.pid"
#define LOCKMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

int daemonize(char * name);
int lockfile(int lock_file);
int daemonAlreadyRunning(int * lock_file);

#endif