#ifndef TRIE_H
#define TRIE_H

#include <semaphore.h>
#include <pthread.h>

#define STR 256
#define MAX_CHILDREN 100

// Structure definition for trie
typedef struct trie {
    char path[STR];
    int server_x;
    int server_y;
    int numchild;
    pthread_rwlock_t rw;
    struct trie* pr;
    struct trie* children[MAX_CHILDREN];
    sem_t deleted;
} trie;

// Function prototypes
trie* create_node(const char* name, trie* parent, int server_x);
int addto(trie* root, const char* path, int server_x);
trie* search_path(trie* root, const char* path);
trie* delete_path(trie* root, const char* path);
int merge_trie(trie* root1, trie* root2);
void destroy_trie(trie* node);
void print_trie(trie* node, int depth);

#endif // TRIE_H
