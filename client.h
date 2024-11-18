#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define NAMING_SERVER_IP "10.2.141.242" // Replace with actual IP of Naming Server
#define NAMING_SERVER_PORT 5050         // Replace with actual port of Naming Server
#define BUFFER_SIZE 1024

int initialize_client();
int send_request(int sock, const char *request);
ssize_t receive_response(int sock, char *buffer, size_t buffer_size);
void error_exit(const char *message);

#endif // CLIENTS_H
