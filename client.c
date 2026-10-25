#include "socket_io.h"

int main(int argc, char *argv[]) {
    socket_handle client_sock = BAD_SOCKET;
    struct sockaddr_in server_addr;
    char buffer[512];
    int port;
    int status = 1;

    if (argc != 3 || (port = parse_port(argv[2])) < 0) {
        fprintf(stderr, "Usage: %s server_ipv4 port_number\n", argv[0]);
        fprintf(stderr, "Example: %s 127.0.0.1 5001\n", argv[0]);
        return 1;
    }
    if (network_start() != 0) return 1;
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == BAD_SOCKET) goto cleanup;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Enter an IPv4 address, such as 127.0.0.1.\n");
        goto cleanup;
    }
    if (connect(client_sock, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) != 0) {
        fprintf(stderr, "Could not connect. Start the server and check its IP/port.\n");
        goto cleanup;
    }

    while (1) {
        if (receive_line(client_sock, buffer, sizeof(buffer)) < 0) {
            fprintf(stderr, "Server disconnected or sent an incomplete message.\n");
            break;
        }
        if (strcmp(buffer, "END") == 0) {
            status = 0;
            break;
        }
        if (strcmp(buffer, "INPUT") == 0) {
            int ch;
            printf("< ");
            fflush(stdout);
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                send_text(client_sock, "quit\n");
                status = 0;
                break;
            }
            /* Reject a long terminal line locally instead of splitting it into moves. */
            if (strchr(buffer, '\n') == NULL && !feof(stdin)) {
                while ((ch = getchar()) != '\n' && ch != EOF) { }
                strcpy(buffer, "invalid\n");
            } else if (strchr(buffer, '\n') == NULL) {
                /* fgets can return a short final line at end of input. */
                strcat(buffer, "\n");
            }
            if (send_text(client_sock, buffer) < 0) {
                fprintf(stderr, "Could not send your choice.\n");
                break;
            }
        } else {
            printf("[server] > %s\n", buffer);
            fflush(stdout);
        }
    }
cleanup:
    if (client_sock != BAD_SOCKET) close_socket(client_sock);
    network_stop();
    return status;
}
