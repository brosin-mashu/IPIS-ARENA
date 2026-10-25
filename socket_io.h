#ifndef SOCKET_IO_H
#define SOCKET_IO_H

/* Linux is the project target. This branch also permits local Windows tests. */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_handle;
#define BAD_SOCKET INVALID_SOCKET
#define close_socket closesocket
static int network_start(void) {
    WSADATA data;
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
typedef int socket_handle;
#define BAD_SOCKET (-1)
#define close_socket close
static int network_start(void) {
    return signal(SIGPIPE, SIG_IGN) == SIG_ERR ? -1 : 0;
}
static void network_stop(void) { }
static int interrupted(void) { return errno == EINTR; }
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TCP can send only part of a message, so continue until all bytes are sent. */
static int send_text(socket_handle sock, const char *text) {
    int sent = 0;
    int length = (int)strlen(text);
    while (sent < length) {
        int count = (int)send(sock, text + sent, length - sent, 0);
        if (count < 0 && interrupted()) continue;
        if (count <= 0) return -1;
        sent += count;
    }
    return 0;
}

/* Each protocol message ends with a newline. Never assume one recv = one line. */
static inline int receive_line(socket_handle sock, char *line, int capacity) {
    int length = 0;
    while (length < capacity - 1) {
        char ch;
        int count = (int)recv(sock, &ch, 1, 0);
        if (count < 0 && interrupted()) continue;
        if (count <= 0) return -1;
        if (ch == '\n') {
            line[length] = '\0';
            return length;
        }
        line[length++] = ch;
    }
    return -1;
}

static int parse_port(const char *text) {
    char *end;
    long port = strtol(text, &end, 10);
    if (text[0] == '\0' || *end != '\0' || port < 1 || port > 65535)
        return -1;
    return (int)port;
}

#endif
