#include "storage_server.h"

int connect_and_register(const char *nm_ip, int nm_port);
void *handle_client(void *arg);
void *handle_naming_server(void *arg);
void handle_read(Client *client);
void handle_write(Client *client);
void handle_meta(Client *client);
void handle_stream(Client *client);
void handle_delete(Client *client);
void handle_zip(Client *client);
void handle_unzip(Client *client);

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
    // close(sock);
    printf("Closed connection to Naming Server.\n");

    // Listen for clients
    struct sockaddr_in ssAddr;
    memset(&ssAddr, 0, sizeof(ssAddr));
    ssAddr.sin_family = AF_INET;
    ssAddr.sin_addr.s_addr = INADDR_ANY;
    ssAddr.sin_port = htons(8081);
    
    int ssSocket;
    if ((ssSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    if (bind(ssSocket, (struct sockaddr *)&ssAddr, sizeof(ssAddr)) < 0) {
        perror("Binding failed");
        return EXIT_FAILURE;
    }

    if(listen(ssSocket, MAX_CLIENTS) < 0) {
        perror("Listening failed");
        return EXIT_FAILURE;
    }

    printf("Storage Server started on port %d\n", ntohs(ssAddr.sin_port));
    int num_clients = 0;
    while(1) {
        struct sockaddr_in newAddr;
        socklen_t addr_size = sizeof(newAddr);
        int newSocket = accept(ssSocket, (struct sockaddr *)&newAddr, &addr_size);
        if(newSocket < 0) {
            perror("Accept failed");
            return EXIT_FAILURE;
        }

        char buffer[BUFFER_SIZE];
        if(recv(newSocket, buffer, BUFFER_SIZE, 0) < 0) {
            perror("Failed to receive data from client");
            return EXIT_FAILURE;
        }
        
        if(strncmp(buffer, "CLIENT", 6) == 0) {
            Client *client = (Client *)malloc(sizeof(Client));
            client->socket = newSocket;
            client->addr = newAddr;
            strcpy(client->ip, inet_ntoa(newAddr.sin_addr));
            client->port = ntohs(newAddr.sin_port);

            pthread_t tid;
            pthread_create(&tid, NULL, handle_client, (void *)client);
            num_clients++;
        } else if(strncmp(buffer, "NS", 2) == 0) {
            NamingServer *ns = (NamingServer *)malloc(sizeof(NamingServer));
            ns->socket = newSocket;
            ns->addr = newAddr;
            strcpy(ns->ip, inet_ntoa(newAddr.sin_addr));
            ns->port = ntohs(newAddr.sin_port);

            pthread_t tid;
            pthread_create(&tid, NULL, handle_naming_server, (void *)ns);
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

    // Calculate number of all paths and subpaths in the directory storage_space
    FILE *fp = popen("find storage_space/*", "r");
    if (fp == NULL) {
        perror("Failed to execute find command");
        close(sock);
        exit(EXIT_FAILURE);
    }

    char temp[MAX_PATH_SIZE];
    memset(temp, 0, sizeof(temp));

    int num_paths = 0;
    char buffer1[MAX_PATH_SIZE * MAX_PATHS];
    memset(buffer1, 0, sizeof(buffer1));
    int cur = 0;
    while (fgets(temp, sizeof(temp), fp) != NULL) {
        int idx = 14;
        cur = num_paths * MAX_PATH_SIZE;
        while (temp[idx] != '\n') {
            buffer1[cur++] = temp[idx++];
        }
        buffer1[cur] = '\0';
        num_paths++;
        memset(temp, 0, sizeof(temp));
    }

    printf("Number of paths: %d\n", num_paths);

    snprintf(buffer, sizeof(buffer), "%d", num_paths);
    if (send(sock, buffer, sizeof(buffer), 0) < 0) {
        perror("Failed to send number of paths to Naming Server");
        close(sock);
        exit(EXIT_FAILURE);
    }

    send(sock, buffer1, sizeof(buffer1), 0);
    for(int i = 0; i < num_paths; i++) {
        printf("%s\n", buffer1 + i * MAX_PATH_SIZE);
    }

    pclose(fp);

    printf("Number of paths: %d\n", num_paths);
    return sock;
}

void *handle_client(void *arg) {
    
    Client *client = (Client *)arg;
    printf("Client %s:%d connected\n", client->ip, client->port);
    while(1){
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client->socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        printf("Client %s:%d disconnected\n", client->ip, client->port);
        printf("here\n");
        close(client->socket);
        return NULL;
    }
    buffer[bytes_received] = '\0';
    printf("Received from client %s:%d - %s\n", client->ip, client->port, buffer);

    int type = 0;
    if(strncmp(buffer, "READ", 4) == 0) {
        handle_read(client);
    } else if(strncmp(buffer, "WRITE", 5) == 0) {
        handle_write(client);
    } else if(strncmp(buffer, "META", 4) == 0) {
        type = 3;
    } else if(strncmp(buffer, "STREAM", 6) == 0) {
        type = 4;
    } else if(strncmp(buffer, "DELETE", 6) == 0) {
        type = 5;
    } else if(strncmp(buffer, "ZIP", 3) == 0) {
        type = 6;
    } else if(strncmp(buffer, "UNZIP", 5) == 0) {
        type = 6;
    } else {
        printf("Invalid request from client %s:%d\n", client->ip, client->port);
        close(client->socket);
        return NULL;
    }
    }

    sleep(1);
    close(client->socket);
    printf("Client %s:%d disconnected\n", client->ip, client->port);
    
    return NULL;
}

void *handle_naming_server(void *arg) {
    NamingServer *ns = (NamingServer *)arg;
    printf("Naming Server %s:%d connected\n", ns->ip, ns->port);
    return NULL;
}

// Function to handle client requests
void handle_read(Client *client) {
    char path[BUFFER_SIZE];
    char confirmation[BUFFER_SIZE];

    // Receive path from client
    recv(client->socket, path, BUFFER_SIZE, 0);
    printf("Received path: %s\n", path);
    char text[BUFFER_SIZE * MAX_FILE_SIZE];
    int present = 0;
    FILE *fp = fopen(path, "r");

    // Send number of lines in file
    if (fp == NULL) {
        // Send -1 if any error occurs
        strcpy(confirmation, "-1");
        present = 1;
        perror("Failed to open file");
    } else {
        int num_lines = 0;
        char buffer[BUFFER_SIZE];
        strcpy(text, "");
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            strcat(text, buffer);
            num_lines++;
        }
        fclose(fp);
        // Send number of lines in file
        sprintf(confirmation, "%d", num_lines);
    }
    send(client->socket, confirmation, strlen(confirmation), 0);
    if(present) {
        return;
    }
    // Send file content
    send(client->socket, text, strlen(text), 0);
}

void handle_write(Client *client) {
    char path[BUFFER_SIZE];
    char data[BUFFER_SIZE];
    char confirmation[BUFFER_SIZE];

    // Receive path and data from client
    recv(client->socket, path, BUFFER_SIZE, 0);
    recv(client->socket, data, BUFFER_SIZE, 0);
    printf("Received path: %s\n", path);
    printf("Received data: %s\n", data);

    // Write data to file
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        // Send -1 if any error occurs
        strcpy(confirmation, "-1");
        perror("Failed to open file");
    } else {
        fputs(data, fp);
        fclose(fp);
        // Send 1 if write is successful
        strcpy(confirmation, "1");
    }
    send(client->socket, confirmation, strlen(confirmation), 0);
}

// Send data of number of lines, number of words, number of characters, date of creation, date of last modification, file size
void handle_meta(Client *client) {
    char path[BUFFER_SIZE];
    char confirmation[BUFFER_SIZE];

    // Receive path from client
    recv(client->socket, path, BUFFER_SIZE, 0);
    printf("Received path: %s\n", path);

    // Send metadata
    struct stat file_stat;
    if (stat(path, &file_stat) < 0) {
        // Send -1 if any error occurs
        strcpy(confirmation, "-1");
        perror("Failed to get file metadata");
    } else {
        // Send metadata: number of lines, number of words, number of characters, date of creation, date of last modification, file size
        char metadata[BUFFER_SIZE];
        // sprintf(metadata, "%ld %ld %ld %ld %ld %ld", file_stat.st_size, file_stat.st_atime, file_stat.st_mtime, file_stat.st_ctime, file_stat.st_nlink, file_stat.st_blocks);
    }
    send(client->socket, confirmation, strlen(confirmation), 0);
}

// Stream audio file
void handle_stream(Client *client) {
    char filename[BUFFER_SIZE];
    recv(client->socket, filename, BUFFER_SIZE, 0);

    char host[16];
    strcpy(host, inet_ntoa(client->addr.sin_addr));

    int port = client->addr.sin_port;

    int server_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &server_addr.sin_addr);

    // Bind and listen
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1) < 0) {
        perror("Listening failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Waiting for connection on %s:%d...\n", host, port);
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("Accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected. Sending file: %s\n", filename);

    // Open the file
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("File open failed");
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Send file data
    ssize_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(client_fd, buffer, bytes_read, 0);
    }

    printf("File sent successfully.\n");

    fclose(file);
    close(client_fd);
    close(server_fd);
}

void handle_delete(Client *client) {
    char path[BUFFER_SIZE];
    char confirmation[BUFFER_SIZE];

    // Receive path from client
    recv(client->socket, path, BUFFER_SIZE, 0);
    printf("Received path: %s\n", path);

    // Delete file
    if (remove(path) < 0) {
        // Send -1 if any error occurs
        strcpy(confirmation, "-1");
        perror("Failed to delete file");
    } else {
        // Send 1 if delete is successful
        strcpy(confirmation, "1");
    }
    send(client->socket, confirmation, strlen(confirmation), 0);

    // After client receives confirmation, client pings naming server and naming server deletes the path from its trie
}