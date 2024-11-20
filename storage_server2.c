#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define MAX_FILE_SIZE 1024
#define MAX_NAME_SIZE 256
#define MAX_PATH_SIZE 256

typedef struct {
    int socket;
    struct sockaddr_in addr;
    char ip[INET_ADDRSTRLEN];
    int port;
} Client;

typedef struct {
    int socket;
    struct sockaddr_in addr;
    char ip[INET_ADDRSTRLEN];
    int port;
} NamingServer;

typedef struct {
    int id;
    char path[MAX_PATH_SIZE];
} Paths;

typedef struct {
    char path[BUFFER_SIZE];
    char data[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];
    int client_port;
} AsyncWriteArgs;

int connect_and_register(const char *nm_ip, int nm_port);
void *handle_client(void *arg);
void *handle_naming_server(void *arg);
void handle_read(Client *client);
void handle_write(Client *client);
void handle_async(Client *client);
void *async_write_thread(void *arg);
void handle_meta(Client *client);
void handle_stream(Client *client);
void handle_delete(Client *client);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <Naming Server IP> <Naming Server Port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *nm_ip = argv[1];
    int nm_port = atoi(argv[2]);

    if (nm_port <= 0) {
        fprintf(stderr, "Invalid port number.\n");
        return EXIT_FAILURE;
    }

    int sock = connect_and_register(nm_ip, nm_port);
    sleep(1);
    close(sock);
    printf("Closed connection to Naming Server.\n");

    // Listen for clients
    struct sockaddr_in ssAddr;
    memset(&ssAddr, 0, sizeof(ssAddr));
    ssAddr.sin_family = AF_INET;
    ssAddr.sin_addr.s_addr = INADDR_ANY;
    ssAddr.sin_port = htons(8082);

    int ssSocket;
    if ((ssSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    if (bind(ssSocket, (struct sockaddr *)&ssAddr, sizeof(ssAddr)) < 0) {
        perror("Binding failed");
        return EXIT_FAILURE;
    }

    if (listen(ssSocket, MAX_CLIENTS) < 0) {
        perror("Listening failed");
        return EXIT_FAILURE;
    }

    printf("Storage Server started on port %d\n", ntohs(ssAddr.sin_port));

    while (1) {
        struct sockaddr_in newAddr;
        socklen_t addr_size = sizeof(newAddr);
        int newSocket = accept(ssSocket, (struct sockaddr *)&newAddr, &addr_size);
        if (newSocket < 0) {
            perror("Accept failed");
            return EXIT_FAILURE;
        }

        char buffer[BUFFER_SIZE];
        if (recv(newSocket, buffer, BUFFER_SIZE, 0) < 0) {
            perror("Failed to receive data from client");
            return EXIT_FAILURE;
        }

        if (strncmp(buffer, "CLIENT", 6) == 0) {
            Client *client = (Client *)malloc(sizeof(Client));
            client->socket = newSocket;
            client->addr = newAddr;
            strcpy(client->ip, inet_ntoa(newAddr.sin_addr));
            client->port = ntohs(newAddr.sin_port);

            pthread_t tid;
            pthread_create(&tid, NULL, handle_client, (void *)client);
        } else {
            printf("Unknown entity connected\n");
            close(newSocket);
        }
    }

    return EXIT_SUCCESS;
}

int connect_and_register(const char *nm_ip, int nm_port) {
    int sock;
    struct sockaddr_in nmAddr;
    char buffer[BUFFER_SIZE];

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure Naming Server address
    memset(&nmAddr, 0, sizeof(nmAddr));
    nmAddr.sin_family = AF_INET;
    nmAddr.sin_port = htons(nm_port);

    if (inet_pton(AF_INET, nm_ip, &nmAddr.sin_addr) <= 0) {
        perror("Invalid Naming Server IP address");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to the Naming Server
    if (connect(sock, (struct sockaddr *)&nmAddr, sizeof(nmAddr)) < 0) {
        perror("Connection to Naming Server failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to Naming Server at %s:%d\n", nm_ip, nm_port);

    // Send the "SS" identifier
    snprintf(buffer, sizeof(buffer), "SS");
    if (send(sock, buffer, strlen(buffer), 0) < 0) {
        perror("Failed to send identifier to Naming Server");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Sent identifier 'SS' to Naming Server.\n");

    // Send available paths
    FILE *fp = popen("find storage_space/*", "r");
    if (fp == NULL) {
        perror("Failed to execute find command");
        close(sock);
        exit(EXIT_FAILURE);
    }

    char temp[MAX_PATH_SIZE];
    memset(temp, 0, sizeof(temp));

    int num_paths = 0;
    while (fgets(temp, sizeof(temp), fp) != NULL) {
        num_paths++;
    }
    pclose(fp);

    fp = popen("find storage_space/*", "r");
    if (fp == NULL) {
        perror("Failed to execute find command");
        close(sock);
        exit(EXIT_FAILURE);
    }

    Paths paths[num_paths];
    for (int i = 0; i < num_paths; i++) {
        paths[i].id = i + 1;
        fgets(temp, sizeof(temp), fp);
        temp[strlen(temp) - 1] = '\0'; // Remove newline
        strcpy(paths[i].path, temp + 13);
    }

    pclose(fp);

    snprintf(buffer, sizeof(buffer), "%d", num_paths);
    if (send(sock, buffer, sizeof(buffer), 0) < 0) {
        perror("Failed to send number of paths to Naming Server");
        close(sock);
        exit(EXIT_FAILURE);
    }

    int ack_count = 0;
    while (ack_count < num_paths) {
        for (int i = 0; i < num_paths; i++) {
            if (send(sock, &paths[i], sizeof(Paths), 0) < 0) {
                perror("Failed to send paths to Naming Server");
                close(sock);
                exit(EXIT_FAILURE);
            }
        }

        recv(sock, buffer, sizeof(buffer), 0);
        sscanf(buffer, "%d", &ack_count);
    }

    printf("Registered %d paths with Naming Server.\n", num_paths);
    return sock;
}

void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    recv(client->socket, buffer, BUFFER_SIZE, 0);

    if (strncmp(buffer, "WRITE", 5) == 0) {
        char flag_buffer[BUFFER_SIZE];
        recv(client->socket, flag_buffer, BUFFER_SIZE, 0);
        int flag = atoi(flag_buffer);

        if (flag) {
            printf("Here\n");
            handle_async(client);
        } else {
            handle_write(client);
        }
    }

    close(client->socket);
    free(client);
    return NULL;
}

void handle_write(Client *client) {
    char path[BUFFER_SIZE];
    char data[BUFFER_SIZE];
    char confirmation[BUFFER_SIZE];

    recv(client->socket, path, BUFFER_SIZE, 0);
    recv(client->socket, data, BUFFER_SIZE, 0);

    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        strcpy(confirmation, "-1");
    } else {
        fputs(data, fp);
        fclose(fp);
        strcpy(confirmation, "1");
    }

    send(client->socket, confirmation, strlen(confirmation), 0);
}

void handle_async(Client *client) {
    char path[BUFFER_SIZE];
    char data[BUFFER_SIZE];
    recv(client->socket, path, BUFFER_SIZE, 0);
    recv(client->socket, data, BUFFER_SIZE, 0);
    printf("%s\n",path);
    printf("%s\n",data);
    AsyncWriteArgs *args = malloc(sizeof(AsyncWriteArgs));
    strcpy(args->path, path);
    strcpy(args->data, data);
    strcpy(args->client_ip, client->ip);
    args->client_port = client->port;

    pthread_t tid;
    pthread_create(&tid, NULL, async_write_thread, (void *)args);

    char confirmation[] = "Acknowledged. Writing asynchronously.";
    send(client->socket, confirmation, strlen(confirmation), 0);
}

void *async_write_thread(void *arg) {
    AsyncWriteArgs *args = (AsyncWriteArgs *)arg;

    FILE *fp = fopen(args->path, "w");
    if (fp == NULL) {
        free(args);
        pthread_exit(NULL);
    }

    for (size_t i = 0; i < strlen(args->data); i++) {
        fputc(args->data[i], fp);
        fflush(fp);
        usleep(100000); // Slow write simulation
    }

    fclose(fp);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(args->client_port);
    inet_pton(AF_INET, args->client_ip, &client_addr.sin_addr);

    connect(sock, (struct sockaddr *)&client_addr, sizeof(client_addr));
    char completion_message[] = "Write operation completed.";
    send(sock, completion_message, strlen(completion_message), 0);
    close(sock);

    free(args);
    pthread_exit(NULL);
}
