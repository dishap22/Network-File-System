#include "client.h"

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

    if (send_request(sock, "CLIENT")<0){
        close(sock);
        error_exit("Failed to send 'CLIENT' message to Naming Serve ");
    }
    printf(" 'CLIENT' message sent to Naming Server\n");
    return sock;
}

// Connect to a Storage Server
int connect_to_storage_server(const char *ss_ip, int ss_port) {
    int sock;
    struct sockaddr_in server_addr;

    // Create a socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        error_exit("Socket creation failed for Storage Server");
    }

    // Set up the Storage Server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ss_port);

    // Convert IP address from text to binary
    if (inet_pton(AF_INET, ss_ip, &server_addr.sin_addr) <= 0) {
        close(sock);
        error_exit("Invalid Storage Server IP address");
    }

    // Establish connection to the Storage Server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        error_exit("Connection to Storage Server failed");
    }

    printf("Connected to Storage Server at %s:%d\n", ss_ip, ss_port);
    return sock;
}

// Send a request to a server
int send_request(int sock, const char *request) {
    if (send(sock, request, strlen(request), 0) < 0) {
        perror("Failed to send request");
        return -1;
    }
    printf("Request sent: %s\n", request);
    return 0;
}

// Receive a response from a server
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

// Handle file operations with the Storage Server
void handle_operations(int ss_sock) {
    char buffer[BUFFER_SIZE];
    char request[BUFFER_SIZE];
        printf("\nChoose an operation:\n");
        printf("1. Read File\n");
        printf("2. Write File\n");
        printf("3. Exit to enter new path\n");
        printf("Enter your choice: ");
        int choice;
        scanf("%d", &choice);

        switch (choice) {
            case 1: { // Read file
                char path[BUFFER_SIZE];
                printf("Enter file path to read: ");
                scanf(" %[^\n]", path); // Read string with spaces
                snprintf(request, sizeof(request), "READ %s", path);
            }

            case 2: { // Write file
                char path[BUFFER_SIZE], data[BUFFER_SIZE];
                printf("Enter file path to write: ");
                scanf(" %[^\n]", path);
                printf("Enter data to write: ");
                scanf(" %[^\n]", data);
                snprintf(request, sizeof(request), "WRITE %s %s", path, data);
            }

            case 3: // Exit to enter new path
                printf("Returning to main menu...\n");
                return;

            default:
                printf("Invalid choice. Try again.\n");
        }

        if (send_request(ss_sock, request) < 0) {
            close(ss_sock);
            return;
        }

        ssize_t received_bytes = receive_response(ss_sock, buffer, BUFFER_SIZE);
        if (received_bytes > 0) {
            printf("Response:\n%s\n", buffer);
        }
}

int main() {
    int nm_sock, ss_sock;
    char buffer[BUFFER_SIZE];
    char ss_ip[INET_ADDRSTRLEN];
    int ss_port;

    nm_sock = initialize_client();

    while (1) {
        char file_path[BUFFER_SIZE];

        // Request file path from the user
        printf("\nEnter the file path (or type 'EXIT' to quit): ");
        scanf(" %[^\n]", file_path); // Read string with spaces

        if (strcasecmp(file_path, "EXIT") == 0) {
            break;
        }

        // Send request to Naming Server for Storage Server details
        snprintf(buffer, sizeof(buffer), "GET_SERVER %s", file_path);
        if (send_request(nm_sock, buffer) < 0) {
            close(nm_sock);
            return EXIT_FAILURE;
        }

        // Receive Storage Server details
        ssize_t received_bytes = receive_response(nm_sock, buffer, BUFFER_SIZE);
        if (received_bytes > 0) {
            printf("Response from Naming Server:\n%s\n", buffer);
        } else {
            printf("Failed to get response from Naming Server. Try again.\n");
            continue;
        }

        // Parse the Storage Server details
        if (sscanf(buffer, "%s %d", ss_ip, &ss_port) != 2) {
            printf("Invalid response from Naming Server. Try again.\n");
            continue;
        }

        // Connect to the Storage Server
        ss_sock = connect_to_storage_server(ss_ip, ss_port);

        // Handle operations with the Storage Server
        handle_operations(ss_sock);

        // Close connection with Storage Server
        close(ss_sock);
    }

    close(nm_sock);
    printf("Connection to Naming Server closed. Exiting...\n");
    return EXIT_SUCCESS;
}
