#ifndef _LRU_H_
#define _LRU_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_SIZE 16

typedef struct CacheNode {
    char *path;
    int ssid;
    struct CacheNode *next;
    struct CacheNode *prev;
} CacheNode;

typedef struct Cache {
    CacheNode *head;
    CacheNode *tail;
    int size;
} LRUCache;

LRUCache *createCache();
CacheNode *createCacheNode(const char *path, int ssid);
void insertCacheNode(LRUCache *cache, const char *path, int ssid);
void removeTail(LRUCache *cache);
void moveNodeToFront(LRUCache *cache, CacheNode *node);
void accessCache(LRUCache *cache, const char *path, int ssid);
void printCache(LRUCache *cache);
int searchCache(LRUCache *cache, const char *path);

#endif