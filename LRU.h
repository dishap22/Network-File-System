#ifndef _LRU_H_
#define _LRU_H_

#define CACHE_SIZE 5

typedef struct CacheNode {
    int ss_index;
    struct CacheNode *next;
    struct CacheNode *prev;
} CacheNode;

typedef struct Cache {
    CacheNode *head;
    CacheNode *tail;
    int size;
    CacheNode *hash[CACHE_SIZE];
} LRUCache;

LRUCache *createCache();
CacheNode *createCacheNode(int ss_index);
void insertCacheNode(LRUCache *cache, int ss_index);
void removeTail(LRUCache *cache);
void moveNodeToFront(LRUCache *cache, CacheNode *node);
void accessCache(LRUCache *cache, int ss_index);
void printCache(LRUCache *cache);

#endif
