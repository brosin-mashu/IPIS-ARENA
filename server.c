#include "socket_io.h"

int main(int argc, char *argv[]) {
    // server_sock accepts connections; players[] holds the two actual connections.
    socket_handle server_sock = BAD_SOCKET;
    socket_handle players[2] = {BAD_SOCKET, BAD_SOCKET};
    struct sockaddr_in server_addr;
    int choices[2] = {0, 0}; // 0 = waiting, 1 = Kagat, 2 = Sagang
    int hp[2] = {50, 50};
    char input[2][64]; // Keep a separate input line for each player.
    int used[2] = {0, 0};
    int too_long[2] = {0, 0};
    int port, i;
    int status = 1; // Change to 0 only after the round finishes successfully.
    int reuse = 1;
    char message[512];

    // Run as ./server 5001. argv[1] is the port typed in the terminal.
    if (argc != 2 || (port = parse_port(argv[1])) < 0) {
        fprintf(stderr, "Usage: %s port_number (1-65535)\n", argv[0]);
        return 1;
    }
    if (network_start() != 0) return 1;
    // IPv4 + stream socket gives us a TCP connection.
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == BAD_SOCKET) goto cleanup;
    // This helps when restarting the server on the same port after closing it.
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&reuse, sizeof(reuse));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Allow local and other reachable PCs.
    server_addr.sin_port = htons((unsigned short)port); // Port needs network byte order.
    // Attach the socket to our address and port before listening.
    if (bind(server_sock, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) != 0) {
        fprintf(stderr, "Could not bind port %d. Is another server using it?\n", port);
        goto cleanup;
    }
    // The 2 here is the backlog. The accept loop below assigns the two players.
    if (listen(server_sock, 2) != 0) goto cleanup;
    printf("IPIS Arena: one-round networking prototype\n");
    printf("Listening on port %d. Waiting for two players...\n", port);
    fflush(stdout);

    // Keep Player 1 connected while waiting for Player 2.
    for (i = 0; i < 2; i++) {
        players[i] = accept(server_sock, NULL, NULL);
        if (players[i] == BAD_SOCKET) goto cleanup;
#ifndef _WIN32
        // select cannot use Linux descriptors outside this range.
        if (players[i] >= FD_SETSIZE) goto cleanup;
#endif
        // Arrays start at 0, so use i + 1 when showing the player number.
        snprintf(message, sizeof(message),
                 "You are Player %d. Waiting for both players.\n", i + 1);
        if (send_text(players[i], message) < 0) goto disconnected;
        printf("Player %d connected.\n", i + 1);
        fflush(stdout);
    }

    // INPUT is a command for the client to show its keyboard prompt.
    for (i = 0; i < 2; i++) {
        if (send_text(players[i],
            "Both players connected. Round 1: P1 HP=50, P2 HP=50.\n"
            "Demo: choose ONE card. 1 = Kagat (5 damage); 2 = Sagang (block).\n"
            "Type quit to leave.\nINPUT\n") < 0) goto disconnected;
    }

    // Do not resolve anything until both players have a valid choice.
    while (choices[0] == 0 || choices[1] == 0) {
        fd_set ready;
        int selected;
        // select changes this set, so rebuild it each time through the loop.
        FD_ZERO(&ready);
        FD_SET(players[0], &ready);
        FD_SET(players[1], &ready);
#ifdef _WIN32
        selected = select(0, &ready, NULL, NULL, NULL);
#else
        // Linux needs the highest descriptor + 1. No timeout for this demo.
        selected = select((players[0] > players[1] ? players[0] : players[1]) + 1,
                          &ready, NULL, NULL, NULL);
#endif
        if (selected < 0 && interrupted()) continue;
        if (selected < 0) goto cleanup;
        for (i = 0; i < 2; i++) {
            char ch;
            int count;
            // Only read from a ready socket, so either player can submit first.
            if (!FD_ISSET(players[i], &ready)) continue;
            count = (int)recv(players[i], &ch, 1, 0);
            if (count < 0 && interrupted()) continue;
            if (count <= 0) goto disconnected; // 0 means the peer closed the connection.
            if (ch != '\n') {
                // Build the line until Enter arrives. Leave space for the string terminator.
                if (ch == '\0') too_long[i] = 1;
                if (used[i] < (int)sizeof(input[i]) - 1) input[i][used[i]++] = ch;
                else too_long[i] = 1;
                continue;
            }
            input[i][used[i]] = '\0';
            // Also accept CRLF line endings.
            if (used[i] > 0 && input[i][used[i] - 1] == '\r')
                input[i][used[i] - 1] = '\0';
            if (!too_long[i] && strcmp(input[i], "quit") == 0)
                goto disconnected;
            // Once submitted, a player cannot replace their choice.
            if (choices[i] != 0) {
                if (send_text(players[i], "Your choice is already locked.\n") < 0)
                    goto disconnected;
            } else if (!too_long[i] &&
                       (strcmp(input[i], "1") == 0 || strcmp(input[i], "2") == 0)) {
                choices[i] = input[i][0] - '0'; // Convert '1' or '2' into an integer.
                // Acknowledge only to this player. Reveal both cards after both submit.
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
            // Reset this input buffer's counters, not the stored card choice.
            used[i] = 0;
            too_long[i] = 0;
        }
    }

    // Kagat deals 5 unless the other player used Sagang. Both Kagat attacks can land.
    if (choices[0] == 1 && choices[1] != 2) hp[1] -= 5;
    if (choices[1] == 1 && choices[0] != 2) hp[0] -= 5;
    snprintf(message, sizeof(message),
        "Round 1 result\nPlayer 1: %s\nPlayer 2: %s\n"
        "Player 1 HP: %d (damage taken: %d)\n"
        "Player 2 HP: %d (damage taken: %d)\n"
        "demo round done\nEND\n",
        choices[0] == 1 ? "Kagat" : "Sagang",
        choices[1] == 1 ? "Kagat" : "Sagang",
        hp[0], 50 - hp[0], hp[1], 50 - hp[1]);
    printf("%s", message);
    // Send the same result to both clients. END tells each client to finish.
    for (i = 0; i < 2; i++) {
        if (send_text(players[i], message) < 0) goto disconnected;
    }
    status = 0;
    goto cleanup;

disconnected:
    // If someone leaves, try to notify both connections before closing them.
    printf("Player %d left or the connection failed. Session ended.\n", i + 1);
    for (i = 0; i < 2; i++) {
        if (players[i] != BAD_SOCKET)
            send_text(players[i], "Session ended: a player left or a connection failed.\nEND\n");
    }
cleanup:
    // All exit paths come here so sockets are not left open.
    if (status != 0) fprintf(stderr, "Server session did not complete.\n");
    for (i = 0; i < 2; i++) {
        if (players[i] != BAD_SOCKET) close_socket(players[i]);
    }
    if (server_sock != BAD_SOCKET) close_socket(server_sock);
    network_stop();
    return status;
}
