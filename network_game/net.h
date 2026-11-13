#ifndef IPIS_NET_H
#define IPIS_NET_H

/* Linux is the target. The Windows branch lets us test the same socket flow. */
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
typedef SOCKET Socket;
#define NO_SOCKET INVALID_SOCKET
#define net_close closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
typedef int Socket;
#define NO_SOCKET (-1)
#define net_close close
#endif

#define PEER_READY 1
#define KEYBOARD_READY 2
#define LINE_SIZE 256

typedef struct {
    char text[LINE_SIZE];
    int used;
    int invalid;
} Line;

int net_start(void);
void net_stop(void);
Socket net_listen(const char *port);
Socket net_connect(const char *host, const char *port);
Socket net_accept(Socket listener);
int net_send(Socket peer, const char *text);
int net_info(Socket peer, const char *text);
int net_wait(Socket peer, int watch_keyboard);
int net_byte(Socket peer, char *ch);
int keyboard_byte(char *ch);
int line_add(Line *line, char ch);
int valid_port(const char *text);
double net_now(void);
int keyboard_start(void);
void keyboard_stop(void);
void keyboard_clear(void);

#endif
