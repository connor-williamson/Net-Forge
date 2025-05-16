#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

void queue_init(queue_t *q, int capacity) {
    q->front = q->rear = NULL;
    q->size = 0;
    q->capacity = capacity;
    q->shutting_down = false;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_destroy(queue_t *q) {
    pthread_mutex_lock(&q->lock);
    queue_node_t *current = q->front;
    while (current) {
        queue_node_t *tmp = current;
        current = current->next;
        free(tmp);
    }
    q->front = q->rear = NULL;
    pthread_mutex_unlock(&q->lock);

    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

bool queue_enqueue(queue_t *q, int client_fd) {
    pthread_mutex_lock(&q->lock);
    while (q->size >= q->capacity && !q->shutting_down) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }

    if (q->shutting_down) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }

    queue_node_t *node = malloc(sizeof(queue_node_t));
    if (!node) {
        perror("malloc");
        pthread_mutex_unlock(&q->lock);
        return false;
    }

    node->client_fd = client_fd;
    node->next = NULL;

    if (!q->rear) {
        q->front = q->rear = node;
    } else {
        q->rear->next = node;
        q->rear = node;
    }

    q->size++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return true;
}

bool queue_dequeue(queue_t *q, int *client_fd) {
    pthread_mutex_lock(&q->lock);
    while (q->size == 0 && !q->shutting_down) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    if (q->shutting_down && q->size == 0) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }

    queue_node_t *node = q->front;
    if (node) {
        *client_fd = node->client_fd;
        q->front = node->next;
        if (!q->front) q->rear = NULL;
        free(node);
        q->size--;
        pthread_cond_signal(&q->not_full);
        pthread_mutex_unlock(&q->lock);
        return true;
    }

    pthread_mutex_unlock(&q->lock);
    return false;
}

void queue_shutdown(queue_t *q) {
    pthread_mutex_lock(&q->lock);
    q->shutting_down = true;
    pthread_cond_broadcast(&q->not_empty); 
    pthread_cond_broadcast(&q->not_full); 
    pthread_mutex_unlock(&q->lock);
}

