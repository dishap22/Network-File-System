#include "data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

trie* create_node(const char* name, trie* parent, int server_x) {
    trie* node = (trie*)malloc(sizeof(trie));
    if (!node) {
        perror("Failed to allocate memory for trie node");
        exit(EXIT_FAILURE);
    }

    // Initialize the path
    strncpy(node->path, name, STR);
    node->path[STR - 1] = '\0'; // Ensure null-termination

    // Initialize server coordinates
    node->server_x = server_x;
    node->server_y = 0; // Default value as it's not set via addto

    // Initialize number of children
    node->numchild = 0;

    // Initialize the read-write lock
    if (pthread_rwlock_init(&node->rw, NULL) != 0) {
        perror("Failed to initialize rwlock");
        free(node);
        exit(EXIT_FAILURE);
    }

    // Set the parent pointer
    node->pr = parent;

    // Initialize the children array to NULL
    for (int i = 0; i < MAX_CHILDREN; ++i) {
        node->children[i] = NULL;
    }

    // Initialize the semaphore (initial value 1 means not deleted)
    if (sem_init(&node->deleted, 0, 1) != 0) {
        perror("Failed to initialize semaphore");
        pthread_rwlock_destroy(&node->rw);
        free(node);
        exit(EXIT_FAILURE);
    }

    return node;
}

trie* init_node(int server_x) {
    return create_node("/", NULL, server_x);
}

int split_path(const char* path, char components[][STR], int max_components) {
    int count = 0;
    char temp[STR * 10]; // Temporary buffer to manipulate the path
    strncpy(temp, path, sizeof(temp));
    temp[sizeof(temp) - 1] = '\0';

    char* token = strtok(temp, "/");
    while (token != NULL && count < max_components) {
        strncpy(components[count++], token, STR);
        components[count - 1][STR - 1] = '\0';
        token = strtok(NULL, "/");
    }

    return count;
}

trie* find_child(trie* parent, const char* name) {
    for (int i = 0; i < parent->numchild; ++i) {
        if (strcmp(parent->children[i]->path, name) == 0) {
            return parent->children[i];
        }
    }
    return NULL;
}

trie* add_child(trie* parent, const char* name, int server_x) {
    if (parent->numchild >= MAX_CHILDREN) {
        fprintf(stderr, "Maximum number of children (%d) reached for node '%s'.\n", MAX_CHILDREN, parent->path);
        return NULL;
    }

    // Initialize the new child node with the provided server_x
    trie* child = create_node(name, parent, server_x);
    parent->children[parent->numchild++] = child;

    return child;
}

int addto(trie* root, const char* path, int server_x) {
    if (root == NULL) {
        fprintf(stderr, "Root node is NULL.\n");
        return -1;
    }

    if (path == NULL || path[0] != '/') {
        fprintf(stderr, "Invalid path. Must start with '/'.\n");
        return -1;
    }

    // Split the path into components
    char components[100][STR]; // Adjust size as needed
    int num_components = split_path(path, components, 100);

    trie* current = root;

    for (int i = 0; i < num_components; ++i) {
        // Acquire read lock to search for the child
        if (pthread_rwlock_rdlock(&current->rw) != 0) {
            perror("Failed to acquire read lock");
            return -1;
        }

        // Try to find the child
        trie* child = find_child(current, components[i]);

        if (child) {
            // Attempt to acquire the deleted semaphore without blocking
            if (sem_trywait(&child->deleted) == 0) {
                // Successfully acquired the semaphore, meaning it's not deleted
                sem_post(&child->deleted); // Release the semaphore

                // Move to the child
                trie* previous = current;
                current = child;

                // Release the read lock of the previous node
                if (pthread_rwlock_unlock(&previous->rw) != 0) {
                    perror("Failed to release read lock");
                    return -1;
                }
            } else {
                // Couldn't acquire the semaphore, meaning it's being deleted
                if (pthread_rwlock_unlock(&current->rw) != 0) {
                    perror("Failed to release read lock");
                    return -1;
                }

                // Acquire write lock to modify the children
                if (pthread_rwlock_wrlock(&current->rw) != 0) {
                    perror("Failed to acquire write lock");
                    return -1;
                }

                // Since we hold the write lock, directly add the child without double-checking
                child = add_child(current, components[i], server_x);
                if (!child) {
                    // Failed to add child (possibly max children reached)
                    pthread_rwlock_unlock(&current->rw);
                    return -1;
                }

                // Move to the child
                current = child;

                // Release the write lock of the parent node
                trie* parent_node = current->pr;
                if (pthread_rwlock_unlock(&parent_node->rw) != 0) {
                    perror("Failed to release write lock");
                    return -1;
                }
            }
        } else {
            // Child does not exist, need to add it
            if (pthread_rwlock_unlock(&current->rw) != 0) {
                perror("Failed to release read lock");
                return -1;
            }

            // Acquire write lock to modify the children
            if (pthread_rwlock_wrlock(&current->rw) != 0) {
                perror("Failed to acquire write lock");
                return -1;
            }

            // Since we hold the write lock, directly add the child without checking
            child = add_child(current, components[i], server_x);
            if (!child) {
                // Failed to add child (possibly max children reached)
                pthread_rwlock_unlock(&current->rw);
                return -1;
            }

            // Move to the child
            current = child;

            // Release the write lock of the parent node
            trie* parent_node = current->pr;
            if (pthread_rwlock_unlock(&parent_node->rw) != 0) {
                perror("Failed to release write lock");
                return -1;
            }
        }
    }

    return 0;
}

trie* search_path(trie* root, const char* path) {
    if (root == NULL) {
        fprintf(stderr, "Root node is NULL.\n");
        return NULL;
    }

    if (path == NULL || path[0] != '/') {
        fprintf(stderr, "Invalid path. Must start with '/'.\n");
        return NULL;
    }

    // Split the path into components
    char components[100][STR]; // Adjust size as needed
    int num_components = split_path(path, components, 100);

    trie* current = root;

    for (int i = 0; i < num_components; ++i) {
        // Acquire read lock to search for the child
        if (pthread_rwlock_rdlock(&current->rw) != 0) {
            perror("Failed to acquire read lock");
            return NULL;
        }

        // Try to find the child
        trie* child = find_child(current, components[i]);

        if (child) {
            // Attempt to acquire the deleted semaphore without blocking
            if (sem_trywait(&child->deleted) == 0) {
                // Successfully acquired the semaphore, meaning it's not deleted
                sem_post(&child->deleted); // Release the semaphore

                // Move to the child
                trie* previous = current;
                current = child;

                // Release the read lock of the previous node
                if (pthread_rwlock_unlock(&previous->rw) != 0) {
                    perror("Failed to release read lock");
                    return NULL;
                }
            } else {
                // Couldn't acquire the semaphore, meaning it's being deleted
                if (pthread_rwlock_unlock(&current->rw) != 0) {
                    perror("Failed to release read lock");
                    return NULL;
                }

                // The node is deleted or being deleted, path does not exist or is inaccessible
                return NULL;
            }
        } else {
            // Child does not exist, path does not exist
            if (pthread_rwlock_unlock(&current->rw) != 0) {
                perror("Failed to release read lock");
                return NULL;
            }
            return NULL;
        }
    }

    // Successfully found all components, return the last node
    return current;
}
trie* delete_path(trie* root, const char* path) {
    if (root == NULL) {
        fprintf(stderr, "Root node is NULL.\n");
        return NULL;
    }

    if (path == NULL || path[0] != '/') {
        fprintf(stderr, "Invalid path. Must start with '/'.\n");
        return NULL;
    }

    // Search for the node corresponding to the path
    trie* node = search_path(root, path);
    if (node == NULL) {
        // Path does not exist or is already deleted
        return NULL;
    }

    // Acquire read lock on the node
    if (pthread_rwlock_rdlock(&node->rw) != 0) {
        perror("Failed to acquire read lock in delete_path");
        return NULL;
    }

    // Attempt to acquire the deleted semaphore without blocking
    if (sem_trywait(&node->deleted) != 0) {
        // Couldn't acquire the semaphore, node is already deleted or being deleted
        if (pthread_rwlock_unlock(&node->rw) != 0) {
            perror("Failed to release read lock after semaphore failure in delete_path");
        }
        return NULL;
    }

    // Successfully acquired semaphore, node is now marked as deleted
    // Do not release the semaphore to indicate deletion

    // Release the read lock
    if (pthread_rwlock_unlock(&node->rw) != 0) {
        perror("Failed to release read lock in delete_path");
        // Note: The semaphore remains acquired to mark deletion
        return NULL;
    }

    return node;
}
void destroy_trie(trie* node) {
    if (node == NULL) {
        return;
    }

    // Acquire write lock to ensure no other thread is accessing this node
    if (pthread_rwlock_wrlock(&node->rw) != 0) {
        perror("Failed to acquire write lock for destruction");
        return;
    }

    // Recursively destroy all children
    for (int i = 0; i < node->numchild; ++i) {
        destroy_trie(node->children[i]);
    }

    // Release and destroy the read-write lock
    if (pthread_rwlock_unlock(&node->rw) != 0) {
        perror("Failed to release write lock during destruction");
    }
    if (pthread_rwlock_destroy(&node->rw) != 0) {
        perror("Failed to destroy rwlock");
    }

    // Destroy the semaphore
    if (sem_destroy(&node->deleted) != 0) {
        perror("Failed to destroy semaphore");
    }

    // Free the node
    free(node);
}

void print_trie(trie* node, int depth) {
    if (node == NULL) {
        return;
    }

    // Attempt to acquire the deleted semaphore without blocking
    if (sem_trywait(&node->deleted) != 0) {
        // Semaphore acquisition failed, node is deleted or being deleted
        if (errno == EAGAIN) {
            // Node is deleted; do not print this node or its children
            return;
        } else {
            perror("sem_trywait failed in print_trie");
            return;
        }
    }

    // Semaphore acquisition succeeded, node is not deleted
    // Immediately release the semaphore to maintain its state
    if (sem_post(&node->deleted) != 0) {
        perror("sem_post failed in print_trie");
        return;
    }

    // Print indentation based on depth
    for (int i = 0; i < depth; ++i) {
        printf("    ");
    }

    // Print node information
    printf("|- %s (server_x: %d, server_y: %d)\n", node->path, node->server_x, node->server_y);

    // Acquire read lock to access children
    if (pthread_rwlock_rdlock(&node->rw) != 0) {
        perror("Failed to acquire read lock in print_trie");
        return;
    }

    // Recursively print each child
    for (int i = 0; i < node->numchild; ++i) {
        print_trie(node->children[i], depth + 1);
    }

    // Release the read lock
    if (pthread_rwlock_unlock(&node->rw) != 0) {
        perror("Failed to release read lock in print_trie");
        return;
    }
}

int merge_trie(trie* root1, trie* root2) {
    if (root1 == NULL || root2 == NULL) {
        fprintf(stderr, "One or both root nodes are NULL. Cannot perform merge.\n");
        return -1;
    }

    // Acquire write lock on root1 to safely modify its children
    if (pthread_rwlock_wrlock(&root1->rw) != 0) {
        perror("Failed to acquire write lock on root1 for merging");
        return -1;
    }

    // Check if root1 has space to add root2 as a child
    if (root1->numchild >= MAX_CHILDREN) {
        fprintf(stderr, "Cannot merge: root1 has reached the maximum number of children (%d).\n", MAX_CHILDREN);
        pthread_rwlock_unlock(&root1->rw);
        return -1;
    }

    // Set root2's parent to root1
    root2->pr = root1;

    // Add root2 as a child of root1
    root1->children[root1->numchild++] = root2;

    // Release the write lock on root1
    if (pthread_rwlock_unlock(&root1->rw) != 0) {
        perror("Failed to release write lock on root1 after merging");
        return -1;
    }

    printf("Successfully merged root2 into root1.\n");
    return 0;
}