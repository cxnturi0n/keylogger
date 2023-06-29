CC = gcc
CFLAGS = -Wall -Wextra

DAEMON_SOURCES = main.c keylogger.c daemon.c
SERVER_SOURCE = server.c

DAEMON_TARGET = keylogger
SERVER_TARGET = server

.PHONY: all clean

all: $(DAEMON_TARGET) $(SERVER_TARGET)

$(DAEMON_TARGET):
	$(CC) $(CFLAGS) $(DAEMON_SOURCES) -o $(DAEMON_TARGET)

$(SERVER_TARGET):
	$(CC) $(CFLAGS) $(SERVER_SOURCE) -o $(SERVER_TARGET)

clean:
	rm -f $(DAEMON_TARGET) $(SERVER_TARGET)
