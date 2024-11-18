#include "client.h"

void error_exit(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

int initialize_client() {
    int sock;
    struct sockaddr_in naming_server_addr;

    // Create the socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        error_exit("Socket creation failed");
    }

    // Set up the server address struct
    memset(&naming_server_addr, 0, sizeof(naming_server_addr));
    naming_server_addr.sin_family = AF_INET;
    naming_server_addr.sin_port = htons(NAMING_SERVER_PORT);

    if (inet_pton(AF_INET, NAMING_SERVER_IP, &naming_server_addr.sin_addr) <= 0) {
        close(sock);
        error_exit("Invalid IP address format");
    }

    // Connect to the Naming Server
    if (connect(sock, (struct sockaddr *)&naming_server_addr, sizeof(naming_server_addr)) < 0) {
        close(sock);
        error_exit("Connection to Naming Server failed");
    }

    printf("Connected to Naming Server at %s:%d\n", NAMING_SERVER_IP, NAMING_SERVER_PORT);
    return sock;
}

int send_request(int sock, const char *request) {
    if (send(sock, request, strlen(request), 0) < 0) {
        perror("Failed to send request");
        return -1;
    }
    printf("Request sent: %s\n", request);
    return 0;
}

ssize_t receive_response(int sock, char *buffer, size_t buffer_size) {
    memset(buffer, 0, buffer_size);
    ssize_t received_bytes = recv(sock, buffer, buffer_size, 0);
    if (received_bytes < 0) {
        perror("Failed to receive response");
        return -1;
    }
    return received_bytes;
}
