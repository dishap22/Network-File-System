#ifndef _CLIENT_H
#define _CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

// Constants
#define NAMING_SERVER_IP "127.0.0.1"  // Replace with Naming Server's actual IP
#define NAMING_SERVER_PORT 8080         // Replace with Naming Server's actual port
#define BUFFER_SIZE 1024

// Function Declarations
void error_exit(const char *message);
int initialize_client();
int send_request(int sock, const char *request);
ssize_t receive_response(int sock, char *buffer, size_t buffer_size);

#endif // CLIENT_H
