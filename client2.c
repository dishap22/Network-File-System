#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define CLIENT_LISTEN_PORT_DEFAULT 9090

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

// Function to send a request to the Storage Server (simple string)
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

// Function to send a request with its length to the Storage Server
int send_request_with_length(int sock, const char *request) {
    size_t length = strlen(request);

    // Send the length of the request in network byte order
    uint32_t length_network_order = htonl(length);
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

    printf("Request sent with length: %s\n", request);
    return 0;
}

// Function to receive a response from the Storage Server
ssize_t receive_response(int sock, char *buffer, size_t buffer_size) {
    memset(buffer, 0, buffer_size);
    ssize_t received_bytes = recv(sock, buffer, buffer_size - 1, 0);
    if (received_bytes < 0) {
        perror("Failed to receive response");
        return -1;
    } else if (received_bytes == 0) {
        printf("Server closed the connection.\n");
        return 0;
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

// Function to set up a listening socket for final acknowledgment
int setup_listening_socket(int listen_port) {
    int listen_sock;
    struct sockaddr_in client_addr;

    // Create a socket
    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Failed to create listening socket");
        return -1;
    }

    // Set socket options to reuse address and port
    int opt = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(listen_sock);
        return -1;
    }

    // Bind the socket to the specified port
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    client_addr.sin_port = htons(listen_port);

    if (bind(listen_sock, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Binding listening socket failed");
        close(listen_sock);
        return -1;
    }

    // Listen for incoming connections
    if (listen(listen_sock, 1) < 0) {
        perror("Listening on socket failed");
        close(listen_sock);
        return -1;
    }

    printf("Client is listening for final acknowledgment on port %d...\n", listen_port);
    return listen_sock;
}

// Function to accept a single connection and receive the final acknowledgment
void *accept_final_ack(void *arg) {
    int listen_sock = *((int *)arg);
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char buffer[BUFFER_SIZE];

    // Accept the incoming connection from the server
    int conn_sock = accept(listen_sock, (struct sockaddr *)&server_addr, &addr_len);
    if (conn_sock < 0) {
        perror("Failed to accept final acknowledgment connection");
        pthread_exit(NULL);
    }

    // Receive the final acknowledgment
    ssize_t bytes_received = recv(conn_sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Final Acknowledgment from Server: %s\n", buffer);
    } else if (bytes_received == 0) {
        printf("Server closed the final acknowledgment connection.\n");
    } else {
        perror("Failed to receive final acknowledgment");
    }

    close(conn_sock);
    pthread_exit(NULL);
}

// Function to perform operations (READ, WRITE, ZIP, UNZIP, DELETE)
void perform_operations(int ss_sock, int listen_port) {
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
        if (scanf("%d", &choice) != 1) {
            fprintf(stderr, "Invalid input. Exiting.\n");
            break;
        }
        getchar(); // Consume the leftover newline character

        if (choice == 6) {
            printf("Exiting...\n");
            break;
        }

        // Get the file path
        printf("Enter file path: ");
        if (fgets(file_path, sizeof(file_path), stdin) == NULL) {
            fprintf(stderr, "Failed to read file path. Exiting.\n");
            break;
        }
        file_path[strcspn(file_path, "\n")] = '\0';

        switch (choice) {
            case 1:
                // READ operation
                if (send_request(ss_sock, "READ") < 0) {
                    fprintf(stderr, "Failed to send READ request.\n");
                    break;
                }
                // Send the listening port if necessary (depends on server implementation)
                // For now, send only the file path
                if (send_request_with_length(ss_sock, file_path) < 0) {
                    fprintf(stderr, "Failed to send file path for READ.\n");
                    break;
                }
                if (receive_response(ss_sock, buffer, BUFFER_SIZE) > 0) {
                    printf("File Content:\n%s\n", buffer);
                }
                break;
            case 2: {
                // WRITE operation
                int async_flag;
                printf("Choose WRITE type:\n1. Synchronous Write\n2. Asynchronous Write\nEnter your choice: ");
                if (scanf("%d", &async_flag) != 1) {
                    fprintf(stderr, "Invalid input for WRITE type.\n");
                    getchar(); // Clear input buffer
                    break;
                }
                getchar(); // Consume the leftover newline character

                if (async_flag != 1 && async_flag != 2) {
                    printf("Invalid WRITE type. Choose 1 or 2.\n");
                    break;
                }

                // Set async_flag to 1 for asynchronous, 0 for synchronous
                int flag_to_send = (async_flag == 2) ? 1 : 0;

                if (send_request(ss_sock, "WRITE") < 0) {
                    fprintf(stderr, "Failed to send WRITE request.\n");
                    break;
                }

                // Send the async_flag in network byte order
                uint32_t flag_network_order = htonl(flag_to_send);
                if (send(ss_sock, &flag_network_order, sizeof(flag_network_order), 0) < 0) {
                    perror("Failed to send async flag");
                    break;
                }

                // Send the file path
                if (send_request_with_length(ss_sock, file_path) < 0) {
                    fprintf(stderr, "Failed to send file path for WRITE.\n");
                    break;
                }

                // Get the data to write
                char data[BUFFER_SIZE];
                printf("Enter data to write to the file: ");
                if (fgets(data, sizeof(data), stdin) == NULL) {
                    fprintf(stderr, "Failed to read data. Skipping WRITE.\n");
                    break;
                }
                data[strcspn(data, "\n")] = '\0';

                // Send the data
                if (send_request_with_length(ss_sock, data) < 0) {
                    fprintf(stderr, "Failed to send data for WRITE.\n");
                    break;
                }

                // Receive the initial acknowledgment
                ssize_t ack_bytes = receive_response(ss_sock, buffer, BUFFER_SIZE);
                if (ack_bytes <= 0) {
                    fprintf(stderr, "Failed to receive initial acknowledgment for WRITE.\n");
                    break;
                }
                printf("Server Response: %s\n", buffer);

                if (flag_to_send == 1) {
                    // Asynchronous Write: Wait for final acknowledgment
                    // Set up a listening socket if not already set
                    // Note: In this implementation, the client sets up the listening socket before connecting
                    // and sends the listening port to the server during the initial "CLIENT" message.

                    // Start a thread to accept the final acknowledgment
                    pthread_t ack_thread;
                    if (pthread_create(&ack_thread, NULL, accept_final_ack, &listen_port) != 0) {
                        perror("Failed to create thread for final acknowledgment");
                        break;
                    }

                    // Detach the thread to allow it to run independently
                    pthread_detach(ack_thread);
                }

                break;
            }
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
                if (send_request(ss_sock, "DELETE") < 0) {
                    fprintf(stderr, "Failed to send DELETE request.\n");
                    break;
                }
                // Send the file path
                if (send_request_with_length(ss_sock, file_path) < 0) {
                    fprintf(stderr, "Failed to send file path for DELETE.\n");
                    break;
                }
                if (receive_response(ss_sock, buffer, BUFFER_SIZE) > 0) {
                    printf("Server Response: %s\n", buffer);
                }
                break;
            default:
                printf("Invalid choice. Try again.\n");
        }
    }
}

int main() {
    char ss_ip[INET_ADDRSTRLEN];
    int ss_port;
    int listen_port;

    // Get Storage Server IP and port from the user
    printf("Enter Storage Server IP: ");
    if (scanf("%s", ss_ip) != 1) {
        fprintf(stderr, "Invalid IP input.\n");
        exit(EXIT_FAILURE);
    }

    printf("Enter Storage Server Port: ");
    if (scanf("%d", &ss_port) != 1 || ss_port <= 0) {
        fprintf(stderr, "Invalid port input.\n");
        exit(EXIT_FAILURE);
    }

    // Consume the leftover newline character
    getchar();

    // Get the listening port for final acknowledgment
    printf("Enter Client Listening Port for Final Acknowledgment (e.g., %d): ", CLIENT_LISTEN_PORT_DEFAULT);
    if (scanf("%d", &listen_port) != 1 || listen_port <= 0 || listen_port > 65535) {
        fprintf(stderr, "Invalid listening port. Using default port %d.\n", CLIENT_LISTEN_PORT_DEFAULT);
        listen_port = CLIENT_LISTEN_PORT_DEFAULT;
    }
    getchar(); // Consume the leftover newline character

    // Set up the listening socket
    int listen_sock = setup_listening_socket(listen_port);
    if (listen_sock < 0) {
        fprintf(stderr, "Failed to set up listening socket. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    // Connect to the Storage Server
    int ss_sock = connect_to_storage_server(ss_ip, ss_port);

    // Send CLIENT message along with the listening port
    // Format: "CLIENT <listen_port>"
    char client_message[BUFFER_SIZE];
    snprintf(client_message, sizeof(client_message), "CLIENT %d", listen_port);
    if (send_request(ss_sock, client_message) < 0) {
        fprintf(stderr, "Failed to send CLIENT message.\n");
        close(ss_sock);
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    // Perform operations (READ, WRITE, ZIP, UNZIP, DELETE)
    perform_operations(ss_sock, listen_port);

    // Close the connection to the Storage Server
    close(ss_sock);
    close(listen_sock);
    printf("Disconnected from Storage Server. Exiting...\n");

    return EXIT_SUCCESS;
}
