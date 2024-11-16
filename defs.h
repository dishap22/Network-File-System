/* LIMITS */
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
} StorageServer;

typedef struct {
    int socket;
    struct sockaddr_in addr;
    char ip[16];
    int port;
} Client;