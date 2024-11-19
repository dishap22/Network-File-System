#include "naming_server.h"

LRUCache *createCache() {
    LRUCache *cache = (LRUCache *)malloc(sizeof(LRUCache));
    cache->head = NULL;
    cache->tail = NULL;
    cache->size = 0;
    return cache;
}

CacheNode *createCacheNode(const char *path, int ssid) {
    CacheNode *node = (CacheNode *)malloc(sizeof(CacheNode));
    node->path = strdup(path);
    node->ssid = ssid;
    node->next = NULL;
    node->prev = NULL;
    return node;
}

void insertCacheNode(LRUCache *cache, const char *path, int ssid) {
    CacheNode *node = createCacheNode(path, ssid);
    if (cache->size == CACHE_SIZE) {
        removeTail(cache);
    }

    node->next = cache->head;
    if (cache->head != NULL) {
        cache->head->prev = node;
    }
    cache->head = node;

    if (cache->tail == NULL) {
        cache->tail = node;
    }

    cache->size++;
}

void removeTail(LRUCache *cache) {
    if (cache->tail == NULL) {
        return;
    }

    CacheNode *node = cache->tail;
    if (cache->tail->prev != NULL) {
        cache->tail = cache->tail->prev;
        cache->tail->next = NULL;
    } else {
        cache->head = NULL;
        cache->tail = NULL;
    }
    free(node->path);
    free(node);
    cache->size--;
}

void moveNodeToFront(LRUCache *cache, CacheNode *node) {
    if (node == cache->head) {
        return;
    }

    if (node == cache->tail) {
        cache->tail = cache->tail->prev;
        cache->tail->next = NULL;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    node->next = cache->head;
    node->prev = NULL;
    if (cache->head != NULL) {
        cache->head->prev = node;
    }
    cache->head = node;
}

void accessCache(LRUCache *cache, const char *path, int ssid) {
    CacheNode *current = cache->head;
    while (current != NULL) {
        if (strcmp(current->path, path) == 0) {
            moveNodeToFront(cache, current);
            return;
        }
        current = current->next;
    }
    insertCacheNode(cache, path, ssid);
}

void printCache(LRUCache *cache) {
    CacheNode *node = cache->head;
    while (node != NULL) {
        printf("%s:%d ", node->path, node->ssid);
        node = node->next;
    }
    printf("\n");
}

int searchCache(LRUCache *cache, const char *path) {
    CacheNode *current = cache->head;
    while (current != NULL) {
        if (strcmp(current->path, path) == 0) {
            moveNodeToFront(cache, current);
            return current->ssid;
        }
        current = current->next;
    }
    return -1; // Indicates not found
}