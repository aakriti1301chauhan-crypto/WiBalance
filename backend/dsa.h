#ifndef WIBALANCE_DSA_H
#define WIBALANCE_DSA_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void *DSAQueueHandle;
typedef void *DSAHashHandle;

DSAQueueHandle dsa_queue_create(void);
void dsa_queue_destroy(DSAQueueHandle);
int dsa_queue_enqueue(DSAQueueHandle, const char *);
int dsa_queue_dequeue(DSAQueueHandle, char *, int);

DSAHashHandle dsa_hash_create(void);
void dsa_hash_destroy(DSAHashHandle);
void dsa_hash_clear(DSAHashHandle);
int dsa_hash_put(DSAHashHandle, const char *, int);
int dsa_hash_get(DSAHashHandle, const char *, int *);

int dsa_linear_search_strings(const char *[], int, const char *);
int dsa_binary_search_strings(const char *[], int, const char *);
void dsa_insertion_sort_indices(int [], double [], int);

#ifdef __cplusplus
}
#endif
#endif
