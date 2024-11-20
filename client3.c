#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define STORAGE_SERVER_PORT 8081     // Hardcoded Storage Server Port

// Function to handle errors and exit the program
void error_exit(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// Function to send the "CLIENT" message to a server
void send_client_message(int sock, const char *server_name) {
    if (send(sock, "CLIENT", strlen("CLIENT"), 0) < 0) {
        fprintf(stderr, "Failed to send CLIENT message to %s\n", server_name);
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("CLIENT message sent to %s\n", server_name);
}

// Connect to the Naming Server
int connect_to_naming_server(const char *ns_ip, int ns_port) {
    int sock;
    struct sockaddr_in server_addr;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        error_exit("Socket creation failed");
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ns_port);

    // Convert IP address from text to binary
    if (inet_pton(AF_INET, ns_ip, &server_addr.sin_addr) <= 0) {
        close(sock);
        error_exit("Invalid Naming Server IP address");
    }

    // Connect to Naming Server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        error_exit("Connection to Naming Server failed");
    }

    printf("Connected to Naming Server at %s:%d\n", ns_ip, ns_port);

    // Send "CLIENT" message to Naming Server
    send_client_message(sock, "Naming Server");

    return sock;
}

// Get Storage Server IP address from the Naming Server
int get_storage_server_ip(int nm_sock, const char *file_path, char *ss_ip) {
    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    // Send GET_SERVER request
    snprintf(request, sizeof(request), "GET_SERVER");
    if (send(nm_sock, request, strlen(request), 0) < 0) {
        perror("Failed to send request to Naming Server");
        return -1;
    }

    snprintf(request, sizeof(request), "%s", file_path);
    if (send(nm_sock, request, strlen(request), 0) < 0) {
        perror("Failed to send request to Naming Server");
        return -1;
    }

    // Receive response
    ssize_t received_bytes = recv(nm_sock, response, sizeof(response) - 1, 0);
    if (received_bytes < 0) {
        perror("Failed to receive response from Naming Server");
        return -1;
    }

    response[received_bytes] = '\0';
    printf("Response from Naming Server: %s\n", response);

    // Extract IP address
    if (sscanf(response, "%s", ss_ip) != 1) {
        fprintf(stderr, "Invalid response format from Naming Server\n");
        return -1;
    }

    return 0;
}

// Function to connect to Storage Server
int connect_to_storage_server(const char *ss_ip) {
    int sock;
    struct sockaddr_in server_addr;

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        error_exit("Socket creation failed");
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(STORAGE_SERVER_PORT);

    // Convert IP address from text to binary
    if (inet_pton(AF_INET, ss_ip, &server_addr.sin_addr) <= 0) {
        close(sock);
        error_exit("Invalid Storage Server IP address");
    }

    // Connect to Storage Server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        error_exit("Connection to Storage Server failed");
    }

    printf("Connected to Storage Server at %s:%d\n", ss_ip, STORAGE_SERVER_PORT);

    // Send "CLIENT" message to Storage Server
    send_client_message(sock, "Storage Server");

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

int send_request1(int sock, const char *request) {
    size_t length = strlen(request);

    // Send the length of the request
    size_t length_network_order = htonl(length);
    if (send(sock, &length_network_order, sizeof(length_network_order), 0) < 0) {
        perror("Failed to send length");
        return -1;
    }

    // Send the actual data
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
    buffer[received_bytes] = '\0';
    return received_bytes;
}


// Function to zip a file
void zip_file(const char *file_path) {
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "zip %s.zip %s", file_path, file_path);
    if (system(command) == 0) {
        printf("File zipped successfully: %s.zip\n", file_path);
    } else {
        fprintf(stderr, "Failed to zip file: %s\n", file_path);
    }
}

// Function to unzip a file
void unzip_file(const char *file_path) {
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "unzip %s", file_path);
    if (system(command) == 0) {
        printf("File unzipped successfully: %s\n", file_path);
    } else {
        fprintf(stderr, "Failed to unzip file: %s\n", file_path);
    }
}

// Function to delete a file
void delete_file(const char *file_path) {
    char command[BUFFER_SIZE];
    snprintf(command, sizeof(command), "rm %s", file_path);
    if (system(command) == 0) {
        printf("File deleted successfully: %s\n", file_path);
    } else {
        fprintf(stderr, "Failed to delete file: %s\n", file_path);
    }
}

void copy_file(const char *src_file, const char *dest_dir) {
    char zip_file[BUFFER_SIZE];
    char dest_file[BUFFER_SIZE];
    char command[BUFFER_SIZE];

    // Step 1: Zip the source file
    snprintf(zip_file, sizeof(zip_file), "%s.zip", src_file);
    snprintf(command, sizeof(command), "zip -j %s %s", zip_file, src_file);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to zip the source file: %s\n", src_file);
        return;
    }
    printf("Zipped file created: %s\n", zip_file);

    // Step 2: Move the zipped file to the destination directory
    snprintf(dest_file, sizeof(dest_file), "%s/%s", dest_dir, strrchr(zip_file, '/') ? strrchr(zip_file, '/') + 1 : zip_file);
    snprintf(command, sizeof(command), "mv %s %s", zip_file, dest_file);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to move the zipped file to the destination directory: %s\n", dest_dir);
        return;
    }
    printf("Zipped file moved to: %s\n", dest_file);

    // Step 3: Unzip the file at the destination
    snprintf(command, sizeof(command), "unzip -o %s -d %s", dest_file, dest_dir);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to unzip the file at the destination: %s\n", dest_dir);
        return;
    }
    printf("File unzipped at destination: %s\n", dest_dir);

    // Step 4: Delete the zipped file at the destination
    snprintf(command, sizeof(command), "rm %s", dest_file);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to delete the zipped file: %s\n", dest_file);
        return;
    }
    printf("Zipped file deleted: %s\n", dest_file);

    printf("File successfully copied from %s to %s\n", src_file, dest_dir);
}

// Placeholder for file operations
void perform_operations(int ss_sock) {
    char buffer[BUFFER_SIZE];
    char file_path[BUFFER_SIZE];
    char dest_path[BUFFER_SIZE];

    while (1) {
        int choice;

        // Display menu
        printf("\nChoose an operation:\n");
        printf("1. Read File\n");
        printf("2. Write File\n");
        printf("3. Zip File\n");
        printf("4. Unzip File\n");
        printf("5. Delete File\n");
        printf("6. Copy File\n");
        printf("7. Stream File\n");
        printf("8. Exit from this storage server\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        getchar(); // Consume the leftover newline character

        if (choice == 8) {
            printf("Exiting...\n");
            break;
        }

        // Get the file path
        printf("Enter file path: ");
        fgets(file_path, sizeof(file_path), stdin);
        file_path[strcspn(file_path, "\n")] = '\0';

        switch (choice) {
            case 1:
                // READ operation
                send_request(ss_sock, "READ");
                send_request1(ss_sock, file_path);
                if (receive_response(ss_sock, buffer, BUFFER_SIZE) > 0) {
                    printf("File Content:\n%s\n", buffer);
                }
                break;
            case 2:
                // WRITE operation
                send_request(ss_sock, "WRITE");
                char data[BUFFER_SIZE];
                printf("Enter data to write to the file: ");
                fgets(data, sizeof(data), stdin);
                data[strcspn(data, "\n")] = '\0';
                send_request1(ss_sock, file_path);
                send_request1(ss_sock, data);
                if (receive_response(ss_sock, buffer, BUFFER_SIZE) > 0) {
                    printf("Server Response:\n%s\n", buffer);
                }
                break;
            case 3:
                // Zip file
                zip_file(file_path);
                break;
            case 4:
                // Unzip file
                unzip_file(file_path);
                break;
            case 5:
                // Delete file
                delete_file(file_path);
                break;
            case 6: 
                printf("Enter destination path: ");
                fgets(dest_path, sizeof(dest_path), stdin);
                dest_path[strcspn(dest_path, "\n")] = '\0';
                copy_file(file_path,dest_path);
                break;
            case 7:
                send_request(ss_sock, "STREAM");
                send_request1(ss_sock, file_path);
                printf("Streaming file...\n");
                FILE *mpv = popen("mpv --no-video -", "w");
                if(!mpv) {
                    perror("Failed to open mpv");
                    close(ss_sock);
                    exit(EXIT_FAILURE);
                }
                ssize_t bytes_received;
                while ((bytes_received = recv(ss_sock, buffer, BUFFER_SIZE, 0)) > 0) {
                    fwrite(buffer, 1, bytes_received, mpv);
                }
                printf("Streaming finished.\n");
                pclose(mpv);
                break;
            default:
                printf("Invalid choice. Try again.\n");
        }
    }
}

int main() {
    char ns_ip[INET_ADDRSTRLEN];
    int ns_port;
    char file_path[BUFFER_SIZE];
    char ss_ip[INET_ADDRSTRLEN];
    int nm_sock, ss_sock;

    // Get Naming Server IP and port from the user
    printf("Enter Naming Server IP: ");
    scanf("%s", ns_ip);
    printf("Enter Naming Server Port: ");
    scanf("%d", &ns_port);

    // Connect to the Naming Server
    nm_sock = connect_to_naming_server(ns_ip, ns_port);

    while (1) {
        // Get file path from the user
        printf("\nEnter the file path (or type 'EXIT' to quit): ");
        getchar(); // Consume leftover newline from previous input
        fgets(file_path, sizeof(file_path), stdin);
        file_path[strcspn(file_path, "\n")] = '\0'; // Remove trailing newline

        if (strcasecmp(file_path, "EXIT") == 0) {
            break;
        }

        // Get Storage Server IP for the given file path
        if (get_storage_server_ip(nm_sock, file_path, ss_ip) < 0) {
            fprintf(stderr, "Failed to get Storage Server details. Try again.\n");
            continue;
        }

        // Connect to the Storage Server
        ss_sock = connect_to_storage_server(ss_ip);

        // Perform operations
        perform_operations(ss_sock);

        // Close the connection to the Storage Server
        close(ss_sock);
        printf("Disconnected from Storage Server\n");
    }

    // Close the connection to the Naming Server
    close(nm_sock);
    printf("Disconnected from Naming Server. Exiting...\n");

    return EXIT_SUCCESS;
}
