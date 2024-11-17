#include "data.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h> // For sleep

// Global root nodes
trie* root1;

// Thread function for deleting a path
void* delete_path_thread(void* arg) {
    const char* delete_path_str = (const char*)arg;
    printf("[Delete Thread] Attempting to delete path: %s\n", delete_path_str);

    if (delete_path(root1, delete_path_str) != NULL) {
        printf("[Delete Thread] Deleted path: %s successfully.\n", delete_path_str);
    } else {
        printf("[Delete Thread] Failed to delete path: %s\n", delete_path_str);
    }

    return NULL;
}

// Thread function for searching a path
void* search_path_thread(void* arg) {
    const char* search_path_str = (const char*)arg;
    //sleep(1); // Delay to allow the delete thread to execute first
    printf("[Search Thread] Attempting to search for path: %s\n", search_path_str);

    trie* result = search_path(root1, search_path_str);
    if (result != NULL) {
        printf("[Search Thread] Found path: %s (Unexpected).\n", search_path_str);
    } else {
        printf("[Search Thread] Search failed for path: %s (Expected).\n", search_path_str);
    }

    printf("[Search Thread] Attempting to search for path: %s\n", search_path_str);

    result = search_path(root1, search_path_str);

    if (result != NULL) {
        printf("[Search Thread] Found path: %s (Unexpected).\n", search_path_str);
    } else {
        printf("[Search Thread] Search failed for path: %s (Expected).\n", search_path_str);
    }

    result = search_path(root1, search_path_str);
    if (result != NULL) {
        printf("[Search Thread] Found path: %s (Unexpected).\n", search_path_str);
    } else {
        printf("[Search Thread] Search failed for path: %s (Expected).\n", search_path_str);
    }

    result = search_path(root1, search_path_str);
    if (result != NULL) {
        printf("[Search Thread] Found path: %s (Unexpected).\n", search_path_str);
    } else {
        printf("[Search Thread] Search failed for path: %s (Expected).\n", search_path_str);
    }

    return NULL;
}

int main() {
    // Example storage server coordinates
    int serverA_x = 1001;

    // Create the first root node (represents "/")
    root1 = create_node("/", NULL, serverA_x);
    if (root1 == NULL) {
        fprintf(stderr, "Failed to create root1 node.\n");
        return -1;
    }

    // Insert initial paths into root1
    addto(root1, "/folder/subfolder/file1.txt", serverA_x);
    addto(root1, "/folder/subfolder/file2.txt", serverA_x);
    addto(root1, "/folder/file3.txt", serverA_x);
    addto(root1, "/anotherfolder/file4.txt", serverA_x);

    // Print the initial trie state
    printf("\n--- Initial Trie State ---\n");
    print_trie(root1, 0);

    // Path to delete and search
    const char* path_to_delete = "/folder/subfolder/file1.txt";

    // Create threads for concurrency testing
    pthread_t thread1, thread2;


    // Thread 2: Search for the same path after a delay
    pthread_create(&thread2, NULL, search_path_thread, (void*)path_to_delete);
    // Thread 1: Delete a path
    pthread_create(&thread1, NULL, delete_path_thread, (void*)path_to_delete);
    // Wait for threads to complete
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    // Print the final trie state
    printf("\n--- Final Trie State ---\n");
    print_trie(root1, 0);

    // Destroy the trie to free memory and resources
    destroy_trie(root1);
    printf("\nDestroyed the trie and freed all resources.\n");

    return 0;
}
