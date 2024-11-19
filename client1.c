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
        printf("8. Exit\n");
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
    char ss_ip[INET_ADDRSTRLEN];
    int ss_port = 8081;

    // Get Storage Server IP and port from the user
    printf("Enter Storage Server IP: ");
    scanf("%s", ss_ip);

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
