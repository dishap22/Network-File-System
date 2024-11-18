#include "headers.h"

// Function to handle errors and exit the program
void error_exit(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// Initialize client and connect to the Naming Server
int initialize_client() {
    int sock;
    struct sockaddr_in server_addr;

    // Create a socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        error_exit("Socket creation failed");
    }

    // Set up the Naming Server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NAMING_SERVER_PORT);

    // Convert IP address from text to binary
    if (inet_pton(AF_INET, NAMING_SERVER_IP, &server_addr.sin_addr) <= 0) {
        close(sock);
        error_exit("Invalid IP address format");
    }

    // Establish connection to the Naming Server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        error_exit("Connection to Naming Server failed");
    }

    printf("Connected to Naming Server at %s:%d\n", NAMING_SERVER_IP, NAMING_SERVER_PORT);
    return sock;
}

// Send a request to the Naming Server
int send_request(int sock, const char *request) {
    if (send(sock, request, strlen(request), 0) < 0) {
        perror("Failed to send request");
        return -1;
    }
    printf("Request sent: %s\n", request);
    return 0;
}

// Receive a response from the Naming Server
ssize_t receive_response(int sock, char *buffer, size_t buffer_size) {
    memset(buffer, 0, buffer_size);
    ssize_t received_bytes = recv(sock, buffer, buffer_size - 1, 0);
    if (received_bytes < 0) {
        perror("Failed to receive response");
        return -1;
    }
    buffer[received_bytes] = '\0'; // Null-terminate the received string
    return received_bytes;
}

// Main function
int main() {
    int sock;
    char buffer[BUFFER_SIZE];

    // Step 1: Initialize the client
    sock = initialize_client();
    if (sock < 0) {
        return EXIT_FAILURE;
    }

    // Step 2: Interact with the Naming Server
    printf("Sending request for accessible paths...\n");
    if (send_request(sock, "LIST_PATHS") < 0) {
        close(sock);
        return EXIT_FAILURE;
    }

    // Step 3: Receive and display the response
    ssize_t received_bytes = receive_response(sock, buffer, BUFFER_SIZE);
    if (received_bytes > 0) {
        printf("Response from Naming Server:\n%s\n", buffer);
    } else {
        close(sock);
        return EXIT_FAILURE;
    }

    // Step 4: Close the connection
    close(sock);
    printf("Connection closed. Exiting...\n");

    return EXIT_SUCCESS;
}
