#ifndef WIBALANCE_DSA_H
#define WIBALANCE_DSA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque C-owned data structures: C++ does not define or use struct types. */
typedef void *DSAQueueHandle;
typedef void *DSAHashHandle;

DSAQueueHandle dsa_queue_create(void);
void dsa_queue_destroy(DSAQueueHandle queue);
int dsa_queue_enqueue(DSAQueueHandle queue, const char *value);
int dsa_queue_dequeue(DSAQueueHandle queue, char *out_value, int out_size);

DSAHashHandle dsa_hash_create(void);
void dsa_hash_destroy(DSAHashHandle table);
void dsa_hash_clear(DSAHashHandle table);
int dsa_hash_put(DSAHashHandle table, const char *key, int value);
int dsa_hash_get(DSAHashHandle table, const char *key, int *out_value);

int dsa_linear_search_strings(const char *items[], int count, const char *target);
int dsa_binary_search_strings(const char *items[], int count, const char *target);

/* Sort indices by associated score, highest score first. Implemented in C. */
void dsa_insertion_sort_indices(int indices[], double scores[], int count);

#ifdef __cplusplus
}
#endif
#endif

