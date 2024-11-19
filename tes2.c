#include "LRU.h"


int main() {
    LRUCache *cache = createCache();
    const char *paths[17] = {
        "/file1.txt", "/file2.txt", "/file3.txt", "/file4.txt", "/file5.txt",
        "/file6.txt", "/file7.txt", "/file8.txt", "/file9.txt", "/file10.txt",
        "/file11.txt", "/file12.txt", "/file13.txt", "/file14.txt", "/file15.txt",
        "/file16.txt", "/file17.txt"
    };

    for(int i = 0; i < 16; i++) {
        accessCache(cache, paths[i],1);
    }

    accessCache(cache, paths[16],1);
    printCache(cache);

    printf("Search /file14.txt: %d\n", searchCache(cache, "/file14.txt"));
    printf("Search /file1.txt: %d\n", searchCache(cache, "/file1.txt"));
    printf("Search /file17.txt: %d\n", searchCache(cache, "/file17.txt"));
    printCache(cache);

    return 0;
}