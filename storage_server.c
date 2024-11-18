#include "storage_server.h"

void connect_and_register(const char *nm_ip, int nm_port) {
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
        int idx = 0;
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
    printf("%s\n", buffer1);

    pclose(fp);

    printf("Number of paths: %d\n", num_paths);
    while(1) {}

    close(sock);
    printf("Disconnected from Naming Server.\n");
}

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

    connect_and_register(nm_ip, nm_port);



    return EXIT_SUCCESS;
}