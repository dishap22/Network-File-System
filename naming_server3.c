#include "naming_server.h"
#include <time.h>
#include <stdlib.h>

int ss_number = 0;
int client_number = 0;
int nmSocket;
Client clients[MAX_CLIENTS];
StorageServer storageServers[MAX_STORAGE_SERVERS];

#define MAX_RESPONSE_SIZE 128

typedef struct {
    int primary;
    int backup;
} BackupPair;

BackupPair backupServers[MAX_STORAGE_SERVERS];

trie *fileStructure;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t ss_mutex = PTHREAD_MUTEX_INITIALIZER;     
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

LRUCache *cache;

int receive_request(int sock, char *buffer, size_t max_len) {
    int32_t len_net;
    int received = recv(sock, &len_net, sizeof(len_net), 0);
    if (received != sizeof(len_net)) return -1;
    int len = ntohl(len_net);
    if (len <= 0 || len >= max_len) return -1;
    int total = 0;
    while (total < len) {
        received = recv(sock, buffer + total, len - total, 0);
        if (received <= 0) return -1;
        total += received;
    }
    buffer[len] = '\0';
    return len;
}

void assign_backup(int server_id) {
    if (ss_number < 2) {
        backupServers[server_id].primary = -1;
        backupServers[server_id].backup = -1;
        return; // Not enough servers for backup assignment
    }

    int backup1, backup2;
    do {
        backup1 = rand() % ss_number;
    } while (backup1 == server_id);

    do {
        backup2 = rand() % ss_number;
    } while (backup2 == server_id || backup2 == backup1);

    backupServers[server_id].primary = backup1;
    backupServers[server_id].backup = backup2;
}

void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char operation[MAX_PATH_SIZE];
    char filepath[MAX_PATH_SIZE];
    
    while (1) {
        if (receive_request(client->socket, operation, MAX_PATH_SIZE) <= 0) {
            printf("Client %s:%d disconnected\n", client->ip, client->port);
            close(client->socket);
            break;
        }
        if (receive_request(client->socket, filepath, MAX_PATH_SIZE) <= 0) {
            printf("Client %s:%d disconnected\n", client->ip, client->port);
            close(client->socket);
            break;
        }
        printf("Received from client %s:%d - Operation: %s, Filepath: %s\n", client->ip, client->port, operation, filepath);
        if (strcmp(operation, "GET_SERVER") == 0) {
            int server_x = -1;
            pthread_mutex_lock(&cache_mutex);
            server_x = searchCache(NULL, filepath);
            pthread_mutex_unlock(&cache_mutex);
            if (server_x == -1) {
                trie* foundNode = search_path(fileStructure, filepath);
                if (foundNode != NULL) {
                    server_x = foundNode->server_x;
                    pthread_mutex_lock(&cache_mutex);
                    insertCacheNode(NULL, filepath, server_x);
                    pthread_mutex_unlock(&cache_mutex);
                } else {
                    int error_code = htonl(FILE_NOT_FOUND);
                    send(client->socket, &error_code, sizeof(error_code), 0);
                    printf("Sent FILE_NOT_FOUND error code to client %s:%d\n", client->ip, client->port);
                    continue;
                }
            }
            char *server_ip = storageServers[server_x].ip;
            int server_port = storageServers[server_x].port;
            char response[MAX_RESPONSE_SIZE];
            snprintf(response, sizeof(response), "SERVER %s:%d", server_ip, server_port);
            int response_len = htonl(strlen(response));
            send(client->socket, &response_len, sizeof(response_len), 0);
            send(client->socket, response, strlen(response), 0);
            printf("Sent server info to client %s:%d - %s\n", client->ip, client->port, response);
        }
        else if (strcmp(operation, "DELETE") == 0) {
            pthread_mutex_lock(&cache_mutex);
            trie* deletedNode = delete_path(fileStructure, filepath);
            pthread_mutex_unlock(&cache_mutex);
            if (deletedNode != NULL) {
                char *server_ip = storageServers[deletedNode->server_x].ip;
                int server_port = storageServers[deletedNode->server_x].port;
                char response[MAX_RESPONSE_SIZE];
                snprintf(response, sizeof(response), "SERVER %s:%d", server_ip, server_port);
                int response_len = htonl(strlen(response));
                send(client->socket, &response_len, sizeof(response_len), 0);
                send(client->socket, response, strlen(response), 0);
                printf("Deleted path and sent server info to client %s:%d - %s\n", client->ip, client->port, response);
            } else {
                int error_code = htonl(FILE_NOT_FOUND);
                send(client->socket, &error_code, sizeof(error_code), 0);
                printf("Sent FILE_NOT_FOUND error code to client %s:%d\n", client->ip, client->port);
            }
        }
    }
    return NULL;
}

void *handle_storage_server(void *arg) {
    StorageServer *ss = (StorageServer *)arg;
    char buffer[MAX_PATH_SIZE];

    while (1) {
        int bytes_received = recv(ss->socket, buffer, MAX_PATH_SIZE - 1, 0);
        if (bytes_received <= 0) {
            printf("Storage Server %s:%d disconnected\n", ss->ip, ss->port);
            close(ss->socket);
            break;
        }
        buffer[bytes_received] = '\0';
        printf("Received from storage server %s:%d - %s\n", ss->ip, ss->port, buffer);
    }

    return NULL;
}

void register_storage_server(int ssSocket, struct sockaddr_in ssAddr) {
    pthread_mutex_lock(&ss_mutex);

    if (ss_number < MAX_STORAGE_SERVERS) {
        StorageServer *ss = &storageServers[ss_number];
        ss->socket = ssSocket;
        ss->addr = ssAddr;
        inet_ntop(AF_INET, &ssAddr.sin_addr, ss->ip, sizeof(ss->ip));
        ss->port = ntohs(ssAddr.sin_port);
        char buffer[MAX_PATH_SIZE];
        recv(ssSocket, buffer, MAX_PATH_SIZE - 1, 0);
        buffer[MAX_PATH_SIZE - 1] = '\0';
        sscanf(buffer, "%d", &ss->num_paths);
        char all_files[MAX_PATHS * MAX_PATH_SIZE + 10];
        recv(ssSocket, all_files, sizeof(all_files) - 1, 0);
        all_files[sizeof(all_files) - 1] = '\0';

        for (int i = 0; i < ss->num_paths; i++) {
            char temp[MAX_PATH_SIZE];
            strcpy(temp, "/");
            strncat(temp, all_files + i * MAX_PATH_SIZE, MAX_PATH_SIZE - 2);
            printf("%s\n", temp);
            addto(fileStructure, temp, ss_number);
        }

        assign_backup(ss_number);

        printf("Backup servers for storage server %d: %d (primary), %d (backup)\n", ss_number,
               backupServers[ss_number].primary, backupServers[ss_number].backup);

        printf("Storage Server %d connected: %s:%d with %d paths\n", ss_number, ss->ip, ss->port, ss->num_paths);

        pthread_t ss_thread;
        if (pthread_create(&ss_thread, NULL, handle_storage_server, (void *)ss) != 0) {
            perror("Storage server thread creation failed");
        } else {
            pthread_detach(ss_thread);
        }

        ss_number++;
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
        Client *client = &clients[client_number++];
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
    srand(time(NULL)); // Initialize random seed for backup assignment

    fileStructure = create_node("/", NULL, -1);
    cache = createCache();

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
        recv(newSocket, buffer, sizeof(buffer) - 1, 0);
        buffer[MAX_PATH_SIZE - 1] = '\0';
        if (strncmp(buffer, "CLIENT", 6) == 0) {
            register_client(newSocket, newAddr);
        } else if (strncmp(buffer, "SS", 2) == 0) {
            register_storage_server(newSocket, newAddr);
        } else {
            printf("Unknown entity connected\n");
            close(newSocket);
        }
    }

    pthread_mutex_destroy(&clients_mutex);
    pthread_mutex_destroy(&ss_mutex);
    pthread_mutex_destroy(&cache_mutex);
    close(nmSocket);
    return 0;
}
