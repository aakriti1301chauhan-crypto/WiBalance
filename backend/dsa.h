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


