#include "headers.h"

/** NM Initialization */
int ss_number = 0; // Number of storage servers
int client_number = 0; // Number of clients
int nmSocket; // Naming server socket

void register_client(int clientSocket, struct sockaddr_in clientAddr) {
    client_number++;
    printf("Client %d connected\n", client_number);
    char clientName[MAX_NAME_SIZE];
    sprintf(clientName, "client%d", client_number);
    send(clientSocket, clientName, strlen(clientName), 0);
    close(clientSocket);
}

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

    if (listen(nmSocket, MAX_CLIENTS) < 0) {
        perror("Listening failed");
        return 1;
    }

    printf("Naming server started on port %d\n", port);
    
    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int newClientSocket = accept(nmSocket, (struct sockaddr *) &clientAddr, &clientAddrLen);
        if (newClientSocket < 0) {
            perror("Accept failed");
            return 1;
        }
        register_client(newClientSocket, clientAddr);
    }

    close(nmSocket);
    return 0;
}