#include "socket_io.h"

int main(int argc, char *argv[]) {
    socket_handle client_sock = BAD_SOCKET;
    struct sockaddr_in server_addr;
    char buffer[512];
    int port;
    int status = 1;

    // Example: ./client 127.0.0.1 5001. Use the server PC's IP on the other computer.
    if (argc != 3 || (port = parse_port(argv[2])) < 0) {
        fprintf(stderr, "Usage: %s server_ipv4 port_number\n", argv[0]);
        fprintf(stderr, "Example: %s 127.0.0.1 5001\n", argv[0]);
        return 1;
    }
    if (network_start() != 0) return 1;
    // Same TCP/IPv4 setup as the server, but this side calls connect.
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == BAD_SOCKET) goto cleanup;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);
    // Convert the typed IP address into the form the socket API expects.
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Enter an IPv4 address, such as 127.0.0.1.\n");
        goto cleanup;
    }
    // The server must already be listening on this IP and port.
    if (connect(client_sock, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) != 0) {
        fprintf(stderr, "Could not connect. Start the server and check its IP/port.\n");
        goto cleanup;
    }

    // The server controls when we show messages, ask for a choice, and stop.
    while (1) {
        if (receive_line(client_sock, buffer, sizeof(buffer)) < 0) {
            fprintf(stderr, "Server disconnected or sent an incomplete message.\n");
            break;
        }
        // END closes the session; it does not mean this player won.
        if (strcmp(buffer, "END") == 0) {
            status = 0;
            break;
        }
        // Only ask for keyboard input when the server sends INPUT.
        if (strcmp(buffer, "INPUT") == 0) {
            int ch;
            printf("< ");
            fflush(stdout); // Show the prompt now, even without a newline.
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                send_text(client_sock, "quit\n"); // No more keyboard input available.
                status = 0;
                break;
            }
            // If the line did not fit, discard the rest so it cannot become another move.
            if (strchr(buffer, '\n') == NULL && !feof(stdin)) {
                while ((ch = getchar()) != '\n' && ch != EOF) { }
                strcpy(buffer, "invalid\n");
            } else if (strchr(buffer, '\n') == NULL) {
                // A short final line at EOF still needs our newline delimiter.
                strcat(buffer, "\n");
            }
            // Send the choice. The server validates it and calculates HP.
            if (send_text(client_sock, buffer) < 0) {
                fprintf(stderr, "Could not send your choice.\n");
                break;
            }
        } else {
            // Anything other than INPUT or END is text to display.
            printf("[server] > %s\n", buffer);
            fflush(stdout);
        }
    }
cleanup:
    // Close our one connection before exiting.
    if (client_sock != BAD_SOCKET) close_socket(client_sock);
    network_stop();
    return status;
}
