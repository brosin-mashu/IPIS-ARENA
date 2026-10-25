#include "socket_io.h"

int main(int argc, char *argv[]) {
    socket_handle server_sock = BAD_SOCKET;
    socket_handle players[2] = {BAD_SOCKET, BAD_SOCKET};
    struct sockaddr_in server_addr;
    int choices[2] = {0, 0};
    int hp[2] = {50, 50};
    char input[2][64];
    int used[2] = {0, 0};
    int too_long[2] = {0, 0};
    int port, i;
    int status = 1;
    int reuse = 1;
    char message[512];

    if (argc != 2 || (port = parse_port(argv[1])) < 0) {
        fprintf(stderr, "Usage: %s port_number (1-65535)\n", argv[0]);
        return 1;
    }
    if (network_start() != 0) return 1;
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == BAD_SOCKET) goto cleanup;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&reuse, sizeof(reuse));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((unsigned short)port);
    if (bind(server_sock, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) != 0) {
        fprintf(stderr, "Could not bind port %d. Is another server using it?\n", port);
        goto cleanup;
    }
    if (listen(server_sock, 2) != 0) goto cleanup;
    printf("IPIS Arena: one-round networking prototype\n");
    printf("Listening on port %d. Waiting for two players...\n", port);
    fflush(stdout);

    /* Unlike LE7 chat, keep the first connection open while accepting the second. */
    for (i = 0; i < 2; i++) {
        players[i] = accept(server_sock, NULL, NULL);
        if (players[i] == BAD_SOCKET) goto cleanup;
#ifndef _WIN32
        if (players[i] >= FD_SETSIZE) goto cleanup;
#endif
        snprintf(message, sizeof(message),
                 "You are Player %d. Waiting for both players.\n", i + 1);
        if (send_text(players[i], message) < 0) goto disconnected;
        printf("Player %d connected.\n", i + 1);
        fflush(stdout);
    }

    for (i = 0; i < 2; i++) {
        if (send_text(players[i],
            "Both players connected. Round 1: P1 HP=50, P2 HP=50.\n"
            "Demo: choose ONE card. 1 = Kagat (5 damage); 2 = Sagang (block).\n"
            "Type quit to leave.\nINPUT\n") < 0) goto disconnected;
    }

    /* select lets either player submit first. Choices stay on the server. */
    while (choices[0] == 0 || choices[1] == 0) {
        fd_set ready;
        int selected;
        FD_ZERO(&ready);
        FD_SET(players[0], &ready);
        FD_SET(players[1], &ready);
#ifdef _WIN32
        selected = select(0, &ready, NULL, NULL, NULL);
#else
        selected = select((players[0] > players[1] ? players[0] : players[1]) + 1,
                          &ready, NULL, NULL, NULL);
#endif
        if (selected < 0 && interrupted()) continue;
        if (selected < 0) goto cleanup;
        for (i = 0; i < 2; i++) {
            char ch;
            int count;
            if (!FD_ISSET(players[i], &ready)) continue;
            count = (int)recv(players[i], &ch, 1, 0);
            if (count < 0 && interrupted()) continue;
            if (count <= 0) goto disconnected;
            if (ch != '\n') {
                if (ch == '\0') too_long[i] = 1;
                if (used[i] < (int)sizeof(input[i]) - 1) input[i][used[i]++] = ch;
                else too_long[i] = 1;
                continue;
            }
            input[i][used[i]] = '\0';
            if (used[i] > 0 && input[i][used[i] - 1] == '\r')
                input[i][used[i] - 1] = '\0';
            if (!too_long[i] && strcmp(input[i], "quit") == 0)
                goto disconnected;
            if (choices[i] != 0) {
                if (send_text(players[i], "Your choice is already locked.\n") < 0)
                    goto disconnected;
            } else if (!too_long[i] &&
                       (strcmp(input[i], "1") == 0 || strcmp(input[i], "2") == 0)) {
                choices[i] = input[i][0] - '0';
                if (send_text(players[i],
                    "Choice locked. Waiting for the other player.\n") < 0)
                    goto disconnected;
                printf("Player %d submitted.\n", i + 1);
                fflush(stdout);
            } else {
                if (send_text(players[i],
                    "Invalid choice. Enter exactly 1 or 2.\nINPUT\n") < 0)
                    goto disconnected;
            }
            used[i] = 0;
            too_long[i] = 0;
        }
    }

    /* Apply both effects before reporting. Sagang blocks Kagat in this slot. */
    if (choices[0] == 1 && choices[1] != 2) hp[1] -= 5;
    if (choices[1] == 1 && choices[0] != 2) hp[0] -= 5;
    snprintf(message, sizeof(message),
        "Round 1 result\nPlayer 1: %s\nPlayer 2: %s\n"
        "Player 1 HP: %d (damage taken: %d)\n"
        "Player 2 HP: %d (damage taken: %d)\n"
        "Prototype round complete. This is not a full match.\nEND\n",
        choices[0] == 1 ? "Kagat" : "Sagang",
        choices[1] == 1 ? "Kagat" : "Sagang",
        hp[0], 50 - hp[0], hp[1], 50 - hp[1]);
    printf("%s", message);
    for (i = 0; i < 2; i++) {
        if (send_text(players[i], message) < 0) goto disconnected;
    }
    status = 0;
    goto cleanup;

disconnected:
    printf("Player %d left or the connection failed. Session ended.\n", i + 1);
    for (i = 0; i < 2; i++) {
        if (players[i] != BAD_SOCKET)
            send_text(players[i], "Session ended: a player left or a connection failed.\nEND\n");
    }
cleanup:
    if (status != 0) fprintf(stderr, "Server session did not complete.\n");
    for (i = 0; i < 2; i++) {
        if (players[i] != BAD_SOCKET) close_socket(players[i]);
    }
    if (server_sock != BAD_SOCKET) close_socket(server_sock);
    network_stop();
    return status;
}
