#include "storage_server.h"

int connect_and_register(const char *nm_ip, int nm_port);
void *handle_client(void *arg);
void *handle_naming_server(void *arg);

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

    int num_paths = 0;
    char buffer1[MAX_PATH_SIZE * MAX_PATHS];
    int cur = 0;
    while (fgets(temp, sizeof(temp), fp) != NULL) {
        int idx = 14;
        while (temp[idx] != '\0') {
            buffer1[cur++] = temp[idx++];
        }
        buffer1[cur] = '\0';
        num_paths++;
    }


    printf("Number of paths: %d\n", num_paths);

    snprintf(buffer, sizeof(buffer), "%d", num_paths);
    if (send(sock, buffer, strlen(buffer), 0) < 0) {
        perror("Failed to send number of paths to Naming Server");
        close(sock);
        exit(EXIT_FAILURE);
    }

    send(sock, buffer1, strlen(buffer1), 0);
    printf("%s", buffer1);

    pclose(fp);

    printf("Number of paths: %d\n", num_paths);
    return sock;
}

void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    printf("Client %s:%d connected\n", client->ip, client->port);

    while(1) {
        char buffer[BUFFER_SIZE];
        int bytes_received = recv(client->socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Client %s:%d disconnected\n", client->ip, client->port);
            close(client->socket);
            break;
        }
        buffer[bytes_received] = '\0';
        printf("Received from client %s:%d - %s\n", client->ip, client->port, buffer);
    }
    return NULL;
}

void *handle_naming_server(void *arg) {
    NamingServer *ns = (NamingServer *)arg;
    printf("Naming Server %s:%d connected\n", ns->ip, ns->port);
    return NULL;
}