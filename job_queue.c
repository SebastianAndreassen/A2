#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "job_queue.h"

int job_queue_init(struct job_queue *job_queue, int capacity) {
  if (capacity <= 0) return -1;

    job_queue->capacity   = capacity;
    job_queue->head       = 0;
    job_queue->tail       = 0;
    job_queue->count      = 0;
    job_queue->destroying = 0;

    job_queue->buffer = malloc(sizeof(void*) * (size_t)capacity);
    if (job_queue->buffer == NULL) {
        return -1; // allocation failed
    }

    pthread_mutex_init(&job_queue->lock, NULL);
    pthread_cond_init(&job_queue->not_full,  NULL);
    pthread_cond_init(&job_queue->not_empty, NULL);

    return 0; // success
  
}

int job_queue_destroy(struct job_queue *job_queue) {
  pthread_mutex_lock(&job_queue->lock);

    // 1) Start shutdown: no more blocking pops; future pushes will fail
    job_queue->destroying = 1;

    // 2) Wake everyone so they can notice 'destroying'
    pthread_cond_broadcast(&job_queue->not_empty); // wake consumers blocked on empty
    pthread_cond_broadcast(&job_queue->not_full);  // wake producers blocked on full

    // 3) Wait until the queue is drained by consumers
    while (job_queue->count > 0) {
        // Each successful pop signals not_full, so this will progress
        pthread_cond_wait(&job_queue->not_full, &job_queue->lock);
    }

    pthread_mutex_unlock(&job_queue->lock);

    // 4) Now it’s safe to tear down OS objects & memory
    pthread_cond_destroy(&job_queue->not_empty);
    pthread_cond_destroy(&job_queue->not_full);
    pthread_mutex_destroy(&job_queue->lock);
    free(job_queue->buffer);
    job_queue->buffer = NULL;

    return 0;
}

int job_queue_push(struct job_queue *job_queue, void *data) {
  // lock the lock to protect elements with shared state e.g. head, tail and buffer
  pthread_mutex_lock(&job_queue->lock);

  // pushing threads are blocked while the queue is full
  while (job_queue->count == job_queue->capacity && !job_queue->destroying) {
      pthread_cond_wait(&job_queue->not_full, &job_queue->lock);
  }

  // cannot push to a queue being destroyed
  if (job_queue->destroying) {
      pthread_mutex_unlock(&job_queue->lock);
      return -1;
  }

  // data is pushed to the tail which then advances
  job_queue->buffer[job_queue->tail] = data;
  job_queue->tail = (job_queue->tail + 1) % job_queue->capacity;
  job_queue->count++;

  // wake threads that are blocked from popping due empty queue
  pthread_cond_signal(&job_queue->not_empty);

  // release lock after altering shared state elements
  pthread_mutex_unlock(&job_queue->lock);
  return 0;
}

int job_queue_pop(struct job_queue *job_queue, void **data) {
  pthread_mutex_lock(&job_queue->lock);

  while (job_queue->count == 0 && !job_queue->destroying) {
        pthread_cond_wait(&job_queue->not_empty, &job_queue->lock);
    }

  if (job_queue->destroying && job_queue->count == 0) {
    pthread_mutex_unlock(&job_queue->lock);
    return -1;
  }

  *data = job_queue->buffer[job_queue->head];
  job_queue->head = (job_queue->head + 1) % job_queue->capacity;
  job_queue->count--;

  pthread_cond_signal(&job_queue->not_full);

  pthread_mutex_unlock(&job_queue->lock);
  return 0;

}
