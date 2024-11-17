#include "headers.h"

LRUCache *createCache() {
    LRUCache *cache = (LRUCache *)malloc(sizeof(LRUCache));
    cache->head = NULL;
    cache->tail = NULL;
    cache->size = 0;
    for (int i = 0; i < CACHE_SIZE; i++) {
        cache->hash[i] = NULL;
    }
    return cache;
}

CacheNode *createCacheNode(int ss_index) {
    CacheNode *node = (CacheNode *)malloc(sizeof(CacheNode));
    node->ss_index = ss_index;
    node->next = NULL;
    node->prev = NULL;
    return node;
}

void insertCacheNode(LRUCache *cache, int ss_index) {
    CacheNode *node = createCacheNode(ss_index);
    if (cache -> size == CACHE_SIZE) {
        removeTail(cache);
    }

    node -> next = cache -> head;
    if (cache -> head != NULL) {
        cache -> head -> prev = node;
    }
    cache -> head = node;

    if (cache -> tail == NULL) {
        cache -> tail = node;
    }

    cache -> hash[ss_index] = node;
    cache -> size++;
}

void removeTail(LRUCache *cache) {
    if (cache -> tail == NULL) {
        return;
    }

    CacheNode *node = cache -> tail;
    if (cache -> tail -> prev != NULL) {
        cache -> tail = cache -> tail -> prev;
        cache -> tail -> next = NULL;
    } else {
        cache -> head = NULL;
        cache -> tail = NULL;
    }
    cache -> hash[node -> ss_index] = NULL;
    free(node);
    cache -> size--;
}

void moveNodeToFront(LRUCache *cache, CacheNode *node) {
    if (node == cache -> head) {
        return;
    }

    if (node == cache -> tail) {
        cache -> tail = cache -> tail -> prev;
        cache -> tail -> next = NULL;
    } else {
        node -> prev -> next = node -> next;
        node -> next -> prev = node -> prev;
    }

    node -> next = cache -> head;
    node -> prev = NULL;
    cache -> head -> prev = node;
    cache -> head = node;
}

void accessCache(LRUCache *cache, int ss_index) {
    CacheNode *node = cache -> hash[ss_index];
    if (node == NULL) {
        insertCacheNode(cache, ss_index);
    } else {
        moveNodeToFront(cache, node);
    }
}

void printCache(LRUCache *cache) {
    CacheNode *node = cache -> head;
    while (node != NULL) {
        printf("%d ", node -> ss_index);
        node = node -> next;
    }
    printf("\n");
}