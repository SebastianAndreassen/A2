#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "job_queue.h"

int job_queue_init(struct job_queue *job_queue, int capacity) {
  // Memory allocate for queue 
  job_queue->buffer = malloc(sizeof(void*) * (size_t)capacity);
  if (!job_queue->buffer || capacity <= 0) {
    return -1; // allocation failed or wanted space is non-existent 
  }
  // Initialize struct values
  job_queue->capacity = capacity;
  job_queue->head = 0;
  job_queue->tail = 0;
  job_queue->count = 0;
  job_queue->destroying = false;
  // Finalize initialization of struct
  pthread_mutex_init(&job_queue->lock, NULL);
  pthread_cond_init(&job_queue->not_full,  NULL);
  pthread_cond_init(&job_queue->not_empty, NULL);
  return 0; 
}

int job_queue_destroy(struct job_queue *job_queue) {
  // Lock mutex
  pthread_mutex_lock(&job_queue->lock);
  // Start shutdown: no more blocking pops; future pushes will fail
  job_queue->destroying = true;
  // Wake everyone so they can notice 'destroying'
  pthread_cond_broadcast(&job_queue->not_empty); // wake consumers blocked on empty
  pthread_cond_broadcast(&job_queue->not_full);  // wake producers blocked on full
  // Wait until the queue is drained by consumers
  while (job_queue->count > 0) {
    // Each successful pop signals not_full, so this will progress
    pthread_cond_wait(&job_queue->not_full, &job_queue->lock);
  }
  // Unloc mutex
  pthread_mutex_unlock(&job_queue->lock);
  // Tear down OS objects & memory
  pthread_cond_destroy(&job_queue->not_empty);
  pthread_cond_destroy(&job_queue->not_full);
  pthread_mutex_destroy(&job_queue->lock);
  free(job_queue->buffer);
  job_queue->buffer = NULL;
  return 0;
}

int job_queue_push(struct job_queue *job_queue, void *data) {
  // Lock the mutex to protect elements with shared state e.g. head, tail and buffer
  pthread_mutex_lock(&job_queue->lock);
  // If the queue is full - block all pushes
  while (job_queue->count == job_queue->capacity && !job_queue->destroying) {
    pthread_cond_wait(&job_queue->not_full, &job_queue->lock);
  }
  // Cannot push to a queue being destroyed
  if (job_queue->destroying) {
    pthread_mutex_unlock(&job_queue->lock);
    return -1;
  }
  // Push the job onto the tail of the queue
  job_queue->buffer[job_queue->tail] = data;
  job_queue->tail = (job_queue->tail + 1) % job_queue->capacity;
  job_queue->count++;
  // Allow threads to pop from the queue again
  pthread_cond_signal(&job_queue->not_empty);
  // Release lock after altering shared state elements
  pthread_mutex_unlock(&job_queue->lock);
  return 0;
}

int job_queue_pop(struct job_queue *job_queue, void **data) {
  // Lock mutex
  pthread_mutex_lock(&job_queue->lock);
  // If the queue is empty - block all pops
  while (job_queue->count == 0 && !job_queue->destroying) {
    pthread_cond_wait(&job_queue->not_empty, &job_queue->lock);
  }
  // Cannot pop from a queue being destroyed
  if (job_queue->destroying && job_queue->count == 0) {
    pthread_mutex_unlock(&job_queue->lock);
    return -1;
  }
  // Pop the job from the head of the queue
  *data = job_queue->buffer[job_queue->head]; // Store pointer into caller's variable 
  job_queue->head = (job_queue->head + 1) % job_queue->capacity;
  job_queue->count--;
  // Allow threads to push to the queue
  pthread_cond_signal(&job_queue->not_full);
  // Unlock the mutex again
  pthread_mutex_unlock(&job_queue->lock);
  return 0;
}