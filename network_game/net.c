#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L
#endif
#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifndef _WIN32
#include <time.h>
#include <termios.h>
#else
#include <conio.h>
#endif

static volatile sig_atomic_t interrupted = 0;
static void on_interrupt(int sig) { (void)sig; interrupted = 1; }
#ifndef _WIN32
static struct termios saved_terminal;
static int terminal_changed = 0;
#endif

double net_now(void) {
#ifdef _WIN32
    return (double)GetTickCount64() / 1000.0;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + now.tv_nsec / 1000000000.0;
#endif
}

void keyboard_stop(void) {
#ifndef _WIN32
    if (terminal_changed) tcsetattr(STDIN_FILENO, TCSANOW, &saved_terminal);
    terminal_changed = 0;
#endif
}

int keyboard_start(void) {
    signal(SIGINT, on_interrupt);
    signal(SIGTERM, on_interrupt);
#ifndef _WIN32
    if (isatty(STDIN_FILENO)) {
        struct termios mode;
        if (tcgetattr(STDIN_FILENO, &saved_terminal) != 0) return -1;
        mode = saved_terminal;
        mode.c_lflag &= ~(ICANON | ECHO);
        mode.c_cc[VMIN] = 1;
        mode.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &mode) != 0) return -1;
        terminal_changed = 1;
        atexit(keyboard_stop);
    }
#endif
    return 0;
}

void keyboard_clear(void) {
#ifdef _WIN32
    if (_isatty(0)) FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
#else
    if (isatty(STDIN_FILENO)) tcflush(STDIN_FILENO, TCIFLUSH);
#endif
}

/* Socket setup is adapted from Hall's server.c and client.c.
   Read CREDITS.md for the source files, changes, and public-domain statement. */
int net_start(void) {
#ifdef _WIN32
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data);
#else
    /* A failed send should be an error we handle, rather than kill the server. */
    return signal(SIGPIPE, SIG_IGN) == SIG_ERR ? -1 : 0;
#endif
}

void net_stop(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

static int was_interrupted(void) {
#ifdef _WIN32
    return WSAGetLastError() == WSAEINTR;
#else
    return errno == EINTR;
#endif
}

Socket net_listen(const char *port) {
    struct addrinfo hints, *servinfo, *p;
    Socket listener = NO_SOCKET;
    int yes = 1;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(NULL, port, &hints, &servinfo) != 0) return NO_SOCKET;

    /* Try the returned addresses until one can be bound, as in Beej's example. */
    for (p = servinfo; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener == NO_SOCKET) continue;
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                       (const char *)&yes, sizeof(yes)) == 0 &&
            bind(listener, p->ai_addr, (int)p->ai_addrlen) == 0) break;
        net_close(listener);
        listener = NO_SOCKET;
    }
    freeaddrinfo(servinfo);
    if (listener != NO_SOCKET && listen(listener, 1) != 0) {
        net_close(listener);
        return NO_SOCKET;
    }
    return listener;
}

Socket net_connect(const char *host, const char *port) {
    struct addrinfo hints, *servinfo, *p;
    Socket peer = NO_SOCKET;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &servinfo) != 0) return NO_SOCKET;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        peer = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (peer == NO_SOCKET) continue;
        if (connect(peer, p->ai_addr, (int)p->ai_addrlen) == 0) break;
        net_close(peer);
        peer = NO_SOCKET;
    }
    freeaddrinfo(servinfo);
    return peer;
}

Socket net_accept(Socket listener) {
    Socket peer;
    do {
        peer = accept(listener, NULL, NULL);
    } while (peer == NO_SOCKET && was_interrupted());
    return peer;
}

/* Adapted from the sendall loop in Beej's guide, section 7.4.
   Here the caller sends a C string and gets 0 or -1, not a byte-count pointer. */
int net_send(Socket peer, const char *text) {
    int total = 0;
    int length = (int)strlen(text);
    while (total < length) {
        int n = (int)send(peer, text + total, length - total, 0);
        if (n < 0 && was_interrupted()) continue;
        if (n <= 0) return -1;
        total += n;
    }
    return 0;
}

/* Prefix display lines so the client can distinguish text from commands. */
int net_info(Socket peer, const char *text) {
    const char *start = text;
    while (*start) {
        const char *end = strchr(start, '\n');
        char line[LINE_SIZE];
        int length = end ? (int)(end - start) : (int)strlen(start);
        if (length > LINE_SIZE - 8) return -1;
        snprintf(line, sizeof(line), "INFO %.*s\n", length, start);
        if (net_send(peer, line) < 0) return -1;
        if (!end) break;
        start = end + 1;
    }
    return 0;
}

#ifdef _WIN32
static int keyboard_ready(void) {
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode, available = 0;
    if (!GetConsoleMode(input, &mode)) {
        /* Test processes use redirected input; no bytes are consumed here. */
        if (!PeekNamedPipe(input, NULL, 0, NULL, &available, NULL)) return 1;
        return available > 0;
    } else {
        return _kbhit();
    }
}
#endif

/* select setup adapted from Beej's select.c / selectserver.c.
   Watch one socket plus the host player's keyboard, rather than broadcast chat. */
int net_wait(Socket peer, int watch_keyboard) {
    fd_set ready;
    int n, events = 0;
    if (interrupted) return -1;
#ifndef _WIN32
    if (peer >= FD_SETSIZE) return -1;
#endif
    FD_ZERO(&ready);
    FD_SET(peer, &ready);
#ifdef _WIN32
    {
        struct timeval delay = {0, 50000};
        n = select(0, &ready, NULL, NULL, &delay);
        if (watch_keyboard && keyboard_ready()) events |= KEYBOARD_READY;
    }
#else
    if (watch_keyboard) FD_SET(STDIN_FILENO, &ready);
    {
        struct timeval delay = {0, 50000};
        n = select(peer + 1, &ready, NULL, NULL, &delay);
    }
    if (n > 0 && watch_keyboard && FD_ISSET(STDIN_FILENO, &ready))
        events |= KEYBOARD_READY;
#endif
    if (n < 0) return was_interrupted() ? 0 : -1;
    if (n > 0 && FD_ISSET(peer, &ready)) events |= PEER_READY;
    return events;
}

int net_byte(Socket peer, char *ch) {
    int n;
    do { n = (int)recv(peer, ch, 1, 0); } while (n < 0 && was_interrupted());
    return n;
}

int keyboard_byte(char *ch) {
#ifdef _WIN32
    if (_isatty(0)) {
        int key = _getch();
        if (key == 0 || key == 224) { _getch(); *ch = ' '; return 1; }
        *ch = key == '\r' ? '\n' : (char)key;
        return 1;
    }
    return _read(0, ch, 1);
#else
    int n;
    do { n = (int)read(STDIN_FILENO, ch, 1); } while (n < 0 && errno == EINTR);
    return n;
#endif
}

/* Each source has its own line buffer. A partial remote line cannot block typing. */
int line_add(Line *line, char ch) {
    if (ch != '\n') {
        if (ch == '\0') line->invalid = 1;
        if (line->used < LINE_SIZE - 1) line->text[line->used++] = ch;
        else line->invalid = 1;
        return 0;
    }
    if (line->used > 0 && line->text[line->used - 1] == '\r') line->used--;
    line->text[line->used] = '\0';
    line->used = 0;
    if (line->invalid) { line->invalid = 0; return -1; }
    return 1;
}

int valid_port(const char *text) {
    unsigned port = 0;
    if (!*text) return 0;
    while (*text) {
        if (*text < '0' || *text > '9') return 0;
        port = port * 10 + (unsigned)(*text++ - '0');
        if (port > 65535) return 0;
    }
    return port != 0;
}
