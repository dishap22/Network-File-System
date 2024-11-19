#include "naming_server.h"

int ss_number = 0;           // Number of storage servers
int client_number = 0;       // Number of clients
int nmSocket;                // Naming server socket
Client clients[MAX_CLIENTS]; // Array of clients
StorageServer storageServers[MAX_STORAGE_SERVERS]; // Array of storage servers

trie *fileStructure;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t ss_mutex = PTHREAD_MUTEX_INITIALIZER;     

// ============================ Interfacing Code ============================
// Function to handle client requests
void *handle_client(void *arg) {
    Client *client = (Client *)arg;

    while (1) {
        char buffer[MAX_PATH_SIZE];
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client->socket, buffer, MAX_PATH_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Client %s:%d disconnected\n", client->ip, client->port);
            close(client->socket);
            break;
        }
        buffer[bytes_received] = '\0';
        printf("Received from client %s:%d - %s\n", client->ip, client->port, buffer);
        if (strncmp(buffer, "GET_SERVER", strlen("GET_SERVER")) == 0) {
            char *filepath = buffer + strlen("GET_SERVER") + 1;
            printf("Processing request for file: %s\n", filepath);

            trie* foundNode = search_path(fileStructure, filepath);
            if (foundNode != NULL) {
                char *server_ip = storageServers[foundNode -> server_x].ip;
                int server_port = storageServers[foundNode -> server_x].port;

                char response[128];
                snprintf(response, sizeof(response), "SERVER %s:%d", server_ip, server_port);

                if (send(client -> socket, response, strlen(response), 0) > 0) {
                    perror("Failed to send response to client");
                } else {
                    printf("Sent server info to client %s:%d - %s\n", client->ip, client->port, response);
                }
            } else {
                int error_code = FILE_NOT_FOUND;
                if (send(client->socket, &error_code, sizeof(error_code), 0) < 0) {
                    perror("Failed to send error code to client");
                } else {
                    printf("Sent FILE_NOT_FOUND error code to client %s:%d\n", client->ip, client->port);
                }
            }
        }
        // other client requests here
    }

    return NULL;
}

// Function to handle storage server requests
void *handle_storage_server(void *arg) {
    StorageServer *ss = (StorageServer *)arg;

    while (1) {
        char buffer[MAX_PATH_SIZE];
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(ss->socket, buffer, MAX_PATH_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Storage Server %s:%d disconnected\n", ss->ip, ss->port);
            close(ss->socket);
            break;
        }
        buffer[bytes_received] = '\0';
        printf("Received from storage server %s:%d - %s\n", ss->ip, ss->port, buffer);
        printf("Bytes received: %d\n", bytes_received);
        // Process storage server messages here
    }

    return NULL;
}

// ============================ Initialization Code ============================
// Function to register a storage server
void register_storage_server(int ssSocket, struct sockaddr_in ssAddr) {
    pthread_mutex_lock(&ss_mutex);

    if (ss_number < MAX_STORAGE_SERVERS) {
        StorageServer *ss = &storageServers[ss_number];
        ss->socket = ssSocket;
        ss->addr = ssAddr;
        inet_ntop(AF_INET, &ssAddr.sin_addr, ss->ip, sizeof(ss->ip));
        ss->port = ntohs(ssAddr.sin_port);

        // Receive additional details from the storage server
        char buffer[MAX_PATH_SIZE];
        recv(ssSocket, buffer, MAX_PATH_SIZE, 0);
        sscanf(buffer, "%d", &ss->num_paths);
        printf("Received %d paths from storage server %d\n", ss->num_paths, ss_number);
        
        char all_files[MAX_PATHS * MAX_PATH_SIZE + 10];
        recv(ssSocket, all_files, MAX_PATHS * MAX_PATH_SIZE + 10, 0);

        for (int i = 0; i < ss->num_paths; i++) {
            char temp[MAX_PATH_SIZE];
            strcpy(temp, "/");
            strcat(temp, all_files + i * MAX_PATH_SIZE);
            printf("%s\n", temp);
            addto(fileStructure, temp, ss_number);
        }

        printf("Files received: \n");
        print_trie(fileStructure, 0);

        printf("Storage Server %d connected: %s:%d with %d paths\n", ss_number, ss->ip, ss->port, ss->num_paths);
        
        pthread_t ss_thread;
        if (pthread_create(&ss_thread, NULL, handle_storage_server, (void *)ss) != 0) {
            perror("Storage server thread creation failed");
        } else {
            pthread_detach(ss_thread);
        }
    } else {
        printf("Max storage servers reached. Connection refused: %s:%d\n", inet_ntoa(ssAddr.sin_addr), ntohs(ssAddr.sin_port));
        close(ssSocket);
    }

    pthread_mutex_unlock(&ss_mutex);
}

// Function to register a client
void register_client(int clientSocket, struct sockaddr_in clientAddr) {
    pthread_mutex_lock(&clients_mutex);

    if (client_number < MAX_CLIENTS) {
        Client *client = &clients[client_number];
        client->socket = clientSocket;
        client->addr = clientAddr;
        inet_ntop(AF_INET, &clientAddr.sin_addr, client->ip, sizeof(client->ip));
        client->port = ntohs(clientAddr.sin_port);

        printf("Client %d connected: %s:%d\n", client_number, client->ip, client->port);

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, (void *)client) != 0) {
            perror("Client thread creation failed");
        } else {
            pthread_detach(client_thread);
        }
    } else {
        printf("Max clients reached. Connection refused: %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
        close(clientSocket);
    }

    pthread_mutex_unlock(&clients_mutex);
}

int main(int argc, char *argv[]) {
    fileStructure = create_node("/", NULL, -1);

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
        struct sockaddr_in newAddr;
        socklen_t newAddrLen = sizeof(newAddr);
        int newSocket = accept(nmSocket, (struct sockaddr *) &newAddr, &newAddrLen);
        if (newSocket < 0) {
            perror("Accept failed");
            continue;
        }

        char buffer[MAX_PATH_SIZE];
        recv(newSocket, buffer, sizeof(buffer), 0);
        if (strncmp(buffer, "CLIENT", 6) == 0) {
            register_client(newSocket, newAddr);
            client_number++;
        } else if (strncmp(buffer, "SS", 2) == 0) {
            register_storage_server(newSocket, newAddr);
            ss_number++;
        } else {
            printf("Unknown entity connected\n");
            close(newSocket);
        }
    }

    pthread_mutex_destroy(&clients_mutex);
    pthread_mutex_destroy(&ss_mutex);
    close(nmSocket);
    return 0;
}