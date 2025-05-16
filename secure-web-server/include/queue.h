#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdbool.h>

typedef struct queue_node {
    int client_fd;
    struct queue_node *next;
} queue_node_t;

typedef struct {
    queue_node_t *front;
    queue_node_t *rear;
    int size;
    int capacity;

    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    bool shutting_down;
} queue_t;

void queue_init(queue_t *q, int capacity);
void queue_destroy(queue_t *q);
bool queue_enqueue(queue_t *q, int client_fd);
bool queue_dequeue(queue_t *q, int *client_fd);
void queue_shutdown(queue_t *q);

#endif
