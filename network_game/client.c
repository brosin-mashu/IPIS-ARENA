#include "net.h"
#include "game.h"
#include <stdio.h>
#include <string.h>

/* Client sends keys with a round ID. Only the server accepts a draft or spends AP. */
int main(int argc, char *argv[]) {
    Socket peer;
    Line remote = {{0}, 0, 0};
    int can_type = 0, greeted = 0, status = 1, round = 0;
    if (argc != 3 || !valid_port(argv[2])) {
        fprintf(stderr, "Usage: %s server_ip port\n", argv[0]);
        return 1;
    }
    if (net_start() != 0) return 1;
    peer = net_connect(argv[1], argv[2]);
    if (peer == NO_SOCKET) {
        puts("Could not connect. Check server IP, port and network.");
        net_stop();
        return 1;
    }
    if (keyboard_start() < 0) goto cleanup;
    puts("You are Player 2. Choices are registered and displayed as you type.");
    for (;;) {
        int events = net_wait(peer, 1);
        if (events < 0) break;
        if (events & PEER_READY) {
            char ch;
            int complete;
            if (net_byte(peer, &ch) <= 0) { puts("Server disconnected."); break; }
            complete = line_add(&remote, ch);
            if (complete < 0) break;
            if (complete > 0) {
                int values[3];
                if (!greeted) {
                    if (strcmp(remote.text, "IPIS-DEMO 2")) {
                        puts("Incompatible protocol; rebuild both programs.");
                        break;
                    }
                    greeted = 1;
                } else if (!strncmp(remote.text, "INFO ", 5)) {
                    puts(remote.text + 5);
                } else if (!strncmp(remote.text, "TURN ", 5) &&
                           parse_numbers(remote.text + 5, values, 3)) {
                    if (values[0] < 1 || values[0] > DEMO_ROUNDS ||
                        values[1] < 1 || values[1] > 2 || values[2] != TURN_SECONDS) break;
                    round = values[0];
                    keyboard_clear();
                    can_type = values[1] == 2;
                    printf("\nRound %d: Player %d planning (%d seconds).\n", round, values[1], values[2]);
                    if (!can_type) puts("Waiting for Player 1; their choices stay hidden.");
                } else if (!strncmp(remote.text, "LOCKED ", 7) &&
                           parse_numbers(remote.text + 7, values, 2)) {
                    if (values[0] != round || values[1] < 1 || values[1] > 2) break;
                    can_type = 0;
                    printf("Player %d plan locked.\n", values[1]);
                } else if (!strcmp(remote.text, "DONE")) {
                    status = 0;
                    break;
                } else if (!strcmp(remote.text, "ABORT")) break;
                else { puts("Unknown server message."); break; }
                fflush(stdout);
            }
        }
        if (events & KEYBOARD_READY) {
            char ch, message[80];
            if (keyboard_byte(&ch) <= 0) break;
            if (ch == 'q' || ch == 'Q' || ch == 3) break;
            if (can_type) {
                snprintf(message, sizeof(message), "KEY %d %d\n", round, (unsigned char)ch);
                if (net_send(peer, message) < 0) break;
                if (ch == '\n' || ch == '\r') can_type = 0;
            }
        }
    }
    if (status != 0) net_send(peer, "QUIT\n");
cleanup:
    keyboard_stop();
    net_close(peer);
    net_stop();
    return status;
}
