#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

// Function to handle errors and exit the program
void error_exit(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// Function to establish a connection to the Storage Server
int connect_to_storage_server(const char *ss_ip, int ss_port) {
    int sock;
    struct sockaddr_in server_addr;

    // Create a socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        error_exit("Socket creation failed");
    }

    // Set up the Storage Server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ss_port);

    // Convert IP address from text to binary
    int inet_result = inet_pton(AF_INET, ss_ip, &server_addr.sin_addr);
    if (inet_result <= 0) {
        close(sock);
        if (inet_result == 0) {
            fprintf(stderr, "Invalid Storage Server IP address format: %s\n", ss_ip);
        } else {
            perror("inet_pton failed");
        }
        exit(EXIT_FAILURE);
    }

    // Establish connection to the Storage Server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        error_exit("Connection to Storage Server failed");
    }

    printf("Connected to Storage Server at %s:%d\n", ss_ip, ss_port);
    return sock;
}

// Function to send a request to the Storage Server
int send_request(int sock, const char *request) {
    size_t length = strlen(request);
    const char *ptr = request;
    size_t remaining = length;

    while (remaining > 0) {
        ssize_t sent = send(sock, ptr, remaining, 0);
        if (sent < 0) {
            perror("Failed to send request");
            return -1;
        }
        ptr += sent;
        remaining -= sent;
    }

    printf("Request sent: %s\n", request);
    return 0;
}

// Function to receive a response from the Storage Server
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

// Function to perform file zipping
void zip_file(const char *file_path) {
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "zip %s.zip %s", file_path, file_path);
    int result = system(command);
    if (result == 0) {
        printf("File successfully zipped: %s.zip\n", file_path);
    } else {
        printf("Failed to zip file: %s\n", file_path);
    }
}

// Function to perform file unzipping
void unzip_file(const char *file_path) {
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "unzip %s.zip", file_path);
    int result = system(command);
    if (result == 0) {
        printf("File successfully unzipped: %s\n", file_path);
    } else {
        printf("Failed to unzip file: %s.zip\n", file_path);
    }
}

// Function to delete a file
void delete_file(const char *file_path) {
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "rm %s", file_path);
    int result = system(command);
    if (result == 0) {
        printf("File successfully deleted: %s\n", file_path);
    } else {
        printf("Failed to delete file: %s\n", file_path);
    }
}

// Function to perform operations with the Storage Server
void perform_operations(int ss_sock) {
    char buffer[BUFFER_SIZE];
    char file_path[BUFFER_SIZE];

    while (1) {
        int choice;
        // Display menu
        printf("\nChoose an operation:\n");
        printf("1. Read File\n");
        printf("2. Write File\n");
        printf("3. Zip File\n");
        printf("4. Unzip File\n");
        printf("5. Delete File\n");
        printf("6. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        getchar(); // Consume the leftover newline character

        if (choice == 6) {
            printf("Exiting...\n");
            break;
        }

        if (choice >= 1 && choice <= 5) {
            printf("Enter file path: ");
            fgets(file_path, sizeof(file_path), stdin);
            file_path[strcspn(file_path, "\n")] = '\0'; // Remove trailing newline
        }

        switch (choice) {
            case 1: // READ
                send_request(ss_sock, "READ");
                send_request(ss_sock, file_path);
                ssize_t received_bytes = receive_response(ss_sock, buffer, BUFFER_SIZE);
                if (received_bytes > 0) {
                    printf("File Content:\n%s\n", buffer);
                } else {
                    printf("Failed to read file.\n");
                }
                break;

            case 2: // WRITE
                send_request(ss_sock, "WRITE");
                char data[BUFFER_SIZE];
                printf("Enter data to write to the file: ");
                fgets(data, sizeof(data), stdin);
                data[strcspn(data, "\n")] = '\0';
                send_request(ss_sock, file_path);
                send_request(ss_sock, data);
                receive_response(ss_sock, buffer, BUFFER_SIZE);
                printf("Server Response: %s\n", buffer);
                break;

            case 3: // ZIP
                zip_file(file_path);
                break;

            case 4: // UNZIP
                unzip_file(file_path);
                break;

            case 5: // DELETE
                delete_file(file_path);
                break;

            default:
                printf("Invalid choice. Try again.\n");
        }
    }
}

int main() {
    char ss_ip[INET_ADDRSTRLEN];
    int ss_port;

    // Get Storage Server IP and port from the user
    printf("Enter Storage Server IP: ");
    scanf("%s", ss_ip);
    printf("Enter Storage Server Port: ");
    scanf("%d", &ss_port);

    // Connect to the Storage Server
    int ss_sock = connect_to_storage_server(ss_ip, ss_port);

    // Send CLIENT message immediately after connection
    if (send_request(ss_sock, "CLIENT") < 0) {
        close(ss_sock);
        return EXIT_FAILURE;
    }

    // Perform operations (READ, WRITE, ZIP, UNZIP, DELETE)
    perform_operations(ss_sock);

    // Close the connection to the Storage Server
    close(ss_sock);
    printf("Disconnected from Storage Server. Exiting...\n");

    return EXIT_SUCCESS;
}
