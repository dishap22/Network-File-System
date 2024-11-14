#include "headers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

int ss_number = 0;           // Number of storage servers
int client_number = 0;       // Number of clients
int nmSocket;                // Naming server socket
int clientSockets[MAX_CLIENTS]; // Client sockets
struct sockaddr_in* clientAddrs[MAX_CLIENTS]; // Client addresses

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for thread safety

// Function to handle communication with a client
void *client_handler(void *arg) {
    int clientSocket = *(int *)arg;
    char buffer[1024];
    int bytesReceived;

    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        printf("Received from client: %s\n", buffer);
        // TEMPORARY returning message to client
        send(clientSocket, buffer, bytesReceived, 0);
    }

    if (bytesReceived == 0) {
        printf("Client disconnected\n");
    } else if (bytesReceived < 0) {
        perror("recv failed");
    }

    close(clientSocket);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_number; i++) {
        if (clientSockets[i] == clientSocket) {
            for (int j = i; j < client_number - 1; j++) {
                clientSockets[j] = clientSockets[j + 1];
                clientAddrs[j] = clientAddrs[j + 1];
            }
            client_number--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    free(arg);
    pthread_exit(NULL);
}

void register_client(int clientSocket, struct sockaddr_in* clientAddr) {
    pthread_mutex_lock(&clients_mutex);
    if (client_number >= MAX_CLIENTS) {
        printf("Max clients reached. Cannot register more clients.\n");
        close(clientSocket);
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

    clientSockets[client_number] = clientSocket;
    clientAddrs[client_number] = clientAddr;
    client_number++;
    pthread_mutex_unlock(&clients_mutex);

    printf("Client %d connected\n", client_number);

    char clientName[MAX_NAME_SIZE];
    sprintf(clientName, "client%d", client_number);
    send(clientSocket, clientName, strlen(clientName), 0);

    // Create a thread to handle communication with the client
    pthread_t tid;
    int *new_sock = malloc(sizeof(int));
    *new_sock = clientSocket;
    if (pthread_create(&tid, NULL, client_handler, (void *)new_sock) != 0) {
        perror("Failed to create thread");
    }
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
        close(nmSocket);
        return 1;
    }

    if (listen(nmSocket, MAX_CLIENTS) < 0) {
        perror("Listening failed");
        close(nmSocket);
        return 1;
    }

    printf("Naming server started on port %d\n", port);

    while (1) {
        struct sockaddr_in* clientAddr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
        socklen_t clientAddrLen = sizeof(struct sockaddr_in);
        int newClientSocket = accept(nmSocket, (struct sockaddr *) clientAddr, &clientAddrLen);
        if (newClientSocket < 0) {
            perror("Accept failed");
            free(clientAddr);
            continue;
        }

        register_client(newClientSocket, clientAddr);
    }

    close(nmSocket);
    return 0;
}
