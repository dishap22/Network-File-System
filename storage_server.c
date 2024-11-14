#include "headers.h"

int ssSocket;

int main(char argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    ssSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (ssSocket < 0) {
        perror("Error creating socket");
        exit(1);
    }

    struct sockaddr_in ssAddr, clientAddr, nmAddr;
    memset(&nmAddr, 0, sizeof(nmAddr));
    memset(&ssAddr, 0, sizeof(ssAddr));
    memset(&clientAddr, 0, sizeof(clientAddr));
    ssAddr.sin_family = AF_INET;
    ssAddr.sin_addr.s_addr = INADDR_ANY;
    ssAddr.sin_port = htons(port);

    if (bind(ssSocket, (struct sockaddr *) &ssAddr, sizeof(ssAddr)) < 0) {
        perror("Error binding socket");
        close(ssSocket);
        exit(1);
    }
    printf("Storage Server started on port %d\n", port);

    if (close(ssSocket) < 0) {
        perror("Error closing socket");
        exit(1);
    }

    return 0;
}