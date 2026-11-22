#include "dsa.h"
#include <stdlib.h>
#include <string.h>

#define KEY_LEN 64
#define HASH_CAPACITY 2048

typedef struct QueueNode {
    char value[KEY_LEN];
    struct QueueNode *next;
} QueueNode;

typedef struct {
    QueueNode *front, *rear;
} Queue;

typedef struct {
    int occupied, value;
    char key[KEY_LEN];
} HashEntry;

typedef struct {
    HashEntry entries[HASH_CAPACITY];
} HashTable;

static unsigned long hash_string(const char *s) {
    unsigned long h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h;
}

DSAQueueHandle dsa_queue_create(void) {
    return calloc(1, sizeof(Queue));
}

void dsa_queue_destroy(DSAQueueHandle handle) {
    Queue *q = (Queue *)handle;
    if (!q) return;
    while (q->front) {
        QueueNode *n = q->front;
        q->front = n->next;
        free(n);
    }
    free(q);
}

int dsa_queue_enqueue(DSAQueueHandle handle, const char *value) {
    Queue *q = (Queue *)handle;
    if (!q || !value) return 0;
    QueueNode *n = (QueueNode *)malloc(sizeof(*n));
    if (!n) return 0;
    strncpy(n->value, value, KEY_LEN - 1);
    n->value[KEY_LEN - 1] = '\0';
    n->next = NULL;
    if (q->rear) q->rear->next = n;
    else q->front = n;
    q->rear = n;
    return 1;
}

int dsa_queue_dequeue(DSAQueueHandle handle, char *out, int size) {
    Queue *q = (Queue *)handle;
    if (!q || !q->front || !out || size <= 0) return 0;
    QueueNode *n = q->front;
    strncpy(out, n->value, (size_t)size - 1);
    out[size - 1] = '\0';
    q->front = n->next;
    if (!q->front) q->rear = NULL;
    free(n);
    return 1;
}

DSAHashHandle dsa_hash_create(void) {
    return calloc(1, sizeof(HashTable));
}

void dsa_hash_destroy(DSAHashHandle handle) { free(handle); }

void dsa_hash_clear(DSAHashHandle handle) {
    if (handle) memset(handle, 0, sizeof(HashTable));
}

int dsa_hash_put(DSAHashHandle handle, const char *key, int value) {
    HashTable *t = (HashTable *)handle;
    if (!t || !key || !*key || strlen(key) >= KEY_LEN) return 0;
    unsigned long start = hash_string(key) % HASH_CAPACITY;
    for (int i = 0; i < HASH_CAPACITY; ++i) {
        HashEntry *e = &t->entries[(start + i) % HASH_CAPACITY];
        if (!e->occupied || !strcmp(e->key, key)) {
            e->occupied = 1;
            strcpy(e->key, key);
            e->value = value;
            return 1;
        }
    }
    return 0;
}

int dsa_hash_get(DSAHashHandle handle, const char *key, int *out) {
    const HashTable *t = (const HashTable *)handle;
    if (!t || !key || !out) return 0;
    unsigned long start = hash_string(key) % HASH_CAPACITY;
    for (int i = 0; i < HASH_CAPACITY; ++i) {
        const HashEntry *e = &t->entries[(start + i) % HASH_CAPACITY];
        if (!e->occupied) return 0;
        if (!strcmp(e->key, key)) { *out = e->value; return 1; }
    }
    return 0;
}

int dsa_linear_search_strings(const char *items[], int count, const char *target) {
    if (!items || !target || count < 0) return -1;
    for (int i = 0; i < count; ++i)
        if (items[i] && !strcmp(items[i], target)) return i;
    return -1;
}

int dsa_binary_search_strings(const char *items[], int count, const char *target) {
    if (!items || !target || count < 0) return -1;
    int low = 0, high = count - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2, cmp = strcmp(items[mid], target);
        if (!cmp) return mid;
        if (cmp < 0) low = mid + 1;
        else high = mid - 1;
    }
    return -1;
}

void dsa_insertion_sort_indices(int indices[], double scores[], int count) {
    if (!indices || !scores || count < 2) return;
    for (int i = 1; i < count; ++i) {
        int index = indices[i], j = i - 1;
        double score = scores[i];
        while (j >= 0 && scores[j] < score) {
            indices[j + 1] = indices[j];
            scores[j + 1] = scores[j];
            --j;
        }
        indices[j + 1] = index;
        scores[j + 1] = score;
    }
}

