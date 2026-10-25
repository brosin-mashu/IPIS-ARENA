// Include this header only once per source compilation.
#ifndef SOCKET_IO_H
#define SOCKET_IO_H

// Ubuntu uses the else branch. Keep this branch for testing on Windows too.
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_handle; // Windows and Linux use different socket types.
#define BAD_SOCKET INVALID_SOCKET
#define close_socket closesocket
static int network_start(void) {
    WSADATA data;
    // Windows needs Winsock initialized before using sockets.
    return WSAStartup(MAKEWORD(2, 2), &data);
}
static void network_stop(void) { WSACleanup(); }
static int interrupted(void) { return WSAGetLastError() == WSAEINTR; }
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
typedef int socket_handle; // On Linux, sockets are integer file descriptors.
#define BAD_SOCKET (-1)
#define close_socket close
static int network_start(void) {
    // A closed connection should give us a send error, not terminate the program.
    return signal(SIGPIPE, SIG_IGN) == SIG_ERR ? -1 : 0;
}
static void network_stop(void) { } // Linux only needs the individual sockets closed.
// Retry an operation if a signal interrupted it.
static int interrupted(void) { return errno == EINTR; }
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// send may write only part of the text. Keep going from the first unsent byte.
static int send_text(socket_handle sock, const char *text) {
    int sent = 0;
    int length = (int)strlen(text);
    while (sent < length) {
        int count = (int)send(sock, text + sent, length - sent, 0);
        if (count < 0 && interrupted()) continue;
        if (count <= 0) return -1;
        sent += count; // Next send starts after the bytes already sent.
    }
    return 0;
}

// TCP does not preserve message boundaries. Our messages end with a newline.
static inline int receive_line(socket_handle sock, char *line, int capacity) {
    int length = 0;
    while (length < capacity - 1) { // Reserve one byte for '\0'.
        char ch;
        int count = (int)recv(sock, &ch, 1, 0);
        if (count < 0 && interrupted()) continue;
        if (count <= 0) return -1;
        if (ch == '\n') {
            line[length] = '\0'; // End the C string without keeping the newline.
            return length;
        }
        line[length++] = ch;
    }
    return -1; // Buffer filled before a complete line arrived.
}

// Read the port from argv and reject empty, out-of-range, or partly numeric input.
static int parse_port(const char *text) {
    char *end;
    long port = strtol(text, &end, 10); // end points to anything left after the number.
    if (text[0] == '\0' || *end != '\0' || port < 1 || port > 65535)
        return -1;
    return (int)port;
}

#endif
