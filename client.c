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

void handle_operations(int sock){
    char buffer[BUFFER_SIZE];
    char request[BUFFER_SIZE];

    while(true){
        printf("\nChoose an Operation:\n");
        printf("1. List Accessible Paths\n");
        printf("2. Read File\n");
        printf("3. Write File\n");
        printf("4. Stream Audio\n");
        printf("5. Create File/Folder\n");
        printf("6. Delete File/Folder\n");
        printf("7. Copy File/Directory\n");
        printf("8. Exit\n");
        printf("Enter your choice: ");
        int choice;
        scanf("%d",&choice);
        switch (choice) {
            case 1: // List accessible paths
                snprintf(request, sizeof(request), "LIST_PATHS");
                break;

            case 2: { // Read file
                char path[BUFFER_SIZE];
                printf("Enter file path to read: ");
                fgets(path, sizeof(path), stdin);
                path[strcspn(path, "\n")] = '\0'; // Remove trailing newline
                snprintf(request, sizeof(request), "READ %s", path);
                break;
            }

            case 3: { // Write file
                char path[BUFFER_SIZE], data[BUFFER_SIZE];
                printf("Enter file path to write: ");
                fgets(path, sizeof(path), stdin);
                path[strcspn(path, "\n")] = '\0';
                printf("Enter data to write: ");
                fgets(data, sizeof(data), stdin);
                data[strcspn(data, "\n")] = '\0';
                snprintf(request, sizeof(request), "WRITE %s %s", path, data);
                break;
            }

            case 4: { // Stream audio
                char path[BUFFER_SIZE];
                printf("Enter audio file path to stream: ");
                fgets(path, sizeof(path), stdin);
                path[strcspn(path, "\n")] = '\0';
                snprintf(request, sizeof(request), "STREAM %s", path);
                break;
            }

            case 5: { // Create file/folder
                char path[BUFFER_SIZE], name[BUFFER_SIZE];
                printf("Enter folder path: ");
                fgets(path, sizeof(path), stdin);
                path[strcspn(path, "\n")] = '\0';
                printf("Enter name of file/folder to create: ");
                fgets(name, sizeof(name), stdin);
                name[strcspn(name, "\n")] = '\0';
                snprintf(request, sizeof(request), "CREATE %s %s", path, name);
                break;
            }

            case 6: { // Delete file/folder
                char path[BUFFER_SIZE];
                printf("Enter file/folder path to delete: ");
                fgets(path, sizeof(path), stdin);
                path[strcspn(path, "\n")] = '\0';
                snprintf(request, sizeof(request), "DELETE %s", path);
                break;
            }

            case 7: { // Copy file/directory
                char source[BUFFER_SIZE], dest[BUFFER_SIZE];
                printf("Enter source path: ");
                fgets(source, sizeof(source), stdin);
                source[strcspn(source, "\n")] = '\0';
                printf("Enter destination path: ");
                fgets(dest, sizeof(dest), stdin);
                dest[strcspn(dest, "\n")] = '\0';
                snprintf(request, sizeof(request), "COPY %s %s", source, dest);
                break;
            }

            case 8: // Exit
                printf("Exiting...\n");
                return;

            default:
                printf("Invalid choice. Try again.\n");
                continue;
        }

        if (send_request(sock, request) < 0) {
            close(sock);
            return;
        }

        ssize_t received_bytes = receive_response(sock, buffer, BUFFER_SIZE);
        if (received_bytes > 0) {
            printf("Response:\n%s\n", buffer);
        }
    }
}

int main() {
    int sock;

    // Step 1: Initialize the client
    sock = initialize_client();
    if (sock < 0) {
        return EXIT_FAILURE;
    }

    // Step 2: Notify the Naming Server of client type
    printf("Sending client type\n");
    if (send_request(sock, "CLIENT") < 0) {
        close(sock);
        return EXIT_FAILURE;
    }

    // Step 3: Handle client operations
    handle_operations(sock);

    // Step 4: Close the connection
    close(sock);
    printf("Connection closed. Exiting...\n");

    return EXIT_SUCCESS;
}