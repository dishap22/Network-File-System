#ifndef _STORAGE_SERVER_H_
#define _STORAGE_SERVER_H_

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <zlib.h>

#define MAX_CLIENTS 100
#define MAX_STORAGE_SERVERS 100
#define MAX_NAME_SIZE 1024 // max length for name of client/server
#define MAX_PATH_SIZE 1024 // max length for path of file
#define MAX_FILE_SIZE 1024 // max length for file content
#define MAX_PATHS 100 // max number of paths in a storage server
#define BUFFER_SIZE 1024
#define FILE_SIZE 1024

typedef struct {
    int socket;
    struct sockaddr_in addr;
    char ip[16];
    int port;
} Client;

typedef struct {
    int socket;
    struct sockaddr_in addr;
    char ip[16];
    int port;
} NamingServer;

typedef struct {
    int id;
    char path[MAX_PATH_SIZE];
} Paths;

#endif