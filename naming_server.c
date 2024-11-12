#include "headers.h"

/** NM Initialization */
int ss_number = 0; // Number of storage servers
int client_number = 0; // Number of clients
int nmSocket; // Naming server socket

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    if ((nmSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0) {
        printf("Invalid port number\n");
        return 1;
    }

    struct sockaddr_in nmAddr;
    memset(&nmAddr, 0, sizeof(nmAddr));
    nmAddr.sin_family = AF_INET;
    nmAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    nmAddr.sin_port = htons(port);

    if (bind(nmSocket, (struct sockaddr *) &nmAddr, sizeof(nmAddr)) < 0) {
        perror("Binding failed");
        return 1;
    }
    printf("Naming server started on port %d\n", port);
    sleep(5);
    close(nmSocket);
}