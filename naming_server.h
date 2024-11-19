#ifndef _NAMING_SERVER_H_
#define _NAMING_SERVER_H_

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
// #include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>

#include "data.h"
#include "LRU.h"
#include "errorcodes.h"

#define MAX_CLIENTS 100
#define MAX_STORAGE_SERVERS 100
#define MAX_NAME_SIZE 1024 // max length for name of client/server
#define MAX_PATH_SIZE 1024 // max length for path of file
#define MAX_FILE_SIZE 1024 // max length for file content
#define MAX_PATHS 100 // max number of paths in a storage server

typedef struct { 
    int socket;
    struct sockaddr_in addr;
    char ip[16];
    int port;
    int num_paths;
    char paths[MAX_PATHS][MAX_PATH_SIZE];
    trie* paths_trie;
} StorageServer;

typedef struct {
    int socket;
    struct sockaddr_in addr;
    char ip[16];
    int port;
} Client;

#endif