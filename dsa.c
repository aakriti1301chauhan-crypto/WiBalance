#include "dsa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DSA_KEY_LEN 64
#define DSA_HASH_CAPACITY 2048

typedef struct QueueNode {
    char value[DSA_KEY_LEN];
    struct QueueNode *next;
} QueueNode;

typedef struct QueueImpl {
    QueueNode *front;
    QueueNode *rear;
    int size;
} QueueImpl;

typedef struct HashEntry {
    int occupied;
    char key[DSA_KEY_LEN];
    int value;
} HashEntry;

typedef struct HashImpl {
    HashEntry entries[DSA_HASH_CAPACITY];
} HashImpl;

static unsigned long hash_string(const char *s) {
    unsigned long h = 5381UL;
    int c;
    while ((c = (unsigned char)*s++)) h = ((h << 5) + h) + (unsigned long)c;
    return h;
}

/* Linked-list FIFO queue */
DSAQueueHandle dsa_queue_create(void) {
    QueueImpl *q = (QueueImpl *)malloc(sizeof(QueueImpl));
    if (!q) return NULL;
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
    return (DSAQueueHandle)q;
}

void dsa_queue_destroy(DSAQueueHandle handle) {
    QueueImpl *q = (QueueImpl *)handle;
    if (!q) return;
    while (q->front) {
        QueueNode *node = q->front;
        q->front = node->next;
        free(node);
    }
    free(q);
}

int dsa_queue_enqueue(DSAQueueHandle handle, const char *value) {
    QueueImpl *q = (QueueImpl *)handle;
    if (!q || !value) return 0;
    QueueNode *node = (QueueNode *)malloc(sizeof(QueueNode));
    if (!node) return 0;
    strncpy(node->value, value, DSA_KEY_LEN - 1);
    node->value[DSA_KEY_LEN - 1] = '\0';
    node->next = NULL;
    if (q->rear) q->rear->next = node;
    else q->front = node;
    q->rear = node;
    q->size++;
    return 1;
}

int dsa_queue_dequeue(DSAQueueHandle handle, char *out_value, int out_size) {
    QueueImpl *q = (QueueImpl *)handle;
    if (!q || !q->front || !out_value || out_size <= 0) return 0;
    QueueNode *node = q->front;
    strncpy(out_value, node->value, (size_t)out_size - 1);
    out_value[out_size - 1] = '\0';
    q->front = node->next;
    if (!q->front) q->rear = NULL;
    free(node);
    q->size--;
    return 1;
}

/* Hash table with linear probing */
DSAHashHandle dsa_hash_create(void) {
    HashImpl *table = (HashImpl *)malloc(sizeof(HashImpl));
    if (!table) return NULL;
    memset(table, 0, sizeof(HashImpl));
    return (DSAHashHandle)table;
}

void dsa_hash_destroy(DSAHashHandle handle) {
    free(handle);
}

void dsa_hash_clear(DSAHashHandle handle) {
    HashImpl *table = (HashImpl *)handle;
    if (table) memset(table, 0, sizeof(HashImpl));
}

int dsa_hash_put(DSAHashHandle handle, const char *key, int value) {
    HashImpl *table = (HashImpl *)handle;
    if (!table || !key || !key[0]) return 0;
    unsigned long start = hash_string(key) % DSA_HASH_CAPACITY;
    for (int i = 0; i < DSA_HASH_CAPACITY; ++i) {
        unsigned long idx = (start + (unsigned long)i) % DSA_HASH_CAPACITY;
        HashEntry *entry = &table->entries[idx];
        if (!entry->occupied || strcmp(entry->key, key) == 0) {
            entry->occupied = 1;
            strncpy(entry->key, key, DSA_KEY_LEN - 1);
            entry->key[DSA_KEY_LEN - 1] = '\0';
            entry->value = value;
            return 1;
        }
    }
    return 0;
}

int dsa_hash_get(DSAHashHandle handle, const char *key, int *out_value) {
    const HashImpl *table = (const HashImpl *)handle;
    if (!table || !key || !out_value) return 0;
    unsigned long start = hash_string(key) % DSA_HASH_CAPACITY;
    for (int i = 0; i < DSA_HASH_CAPACITY; ++i) {
        unsigned long idx = (start + (unsigned long)i) % DSA_HASH_CAPACITY;
        const HashEntry *entry = &table->entries[idx];
        if (!entry->occupied) return 0;
        if (strcmp(entry->key, key) == 0) {
            *out_value = entry->value;
            return 1;
        }
    }
    return 0;
}

/* Linear search */
int dsa_linear_search_strings(const char *items[], int count, const char *target) {
    if (!items || !target) return -1;
    for (int i = 0; i < count; ++i) {
        if (items[i] && strcmp(items[i], target) == 0) return i;
    }
    return -1;
}
