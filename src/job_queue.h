#ifndef JOB_QUEUE_H
#define JOB_QUEUE_H

#include <pthread.h>
#include <stdbool.h>

struct job_queue {
  void **buffer;           // Circular buffer
  int capacity;            // Max number of elements
  int head;                // Next pop index
  int tail;                // Next push index
  int count;               // Number of elements
  bool destroying;         // Flag set by destroy()

  pthread_mutex_t lock;
  pthread_cond_t not_full;
  pthread_cond_t not_empty;
};

struct worker {
  struct job_queue *jq;
  const char *needle;
};

// Initialise a job queue with the given capacity.  The queue starts out
// empty.  Returns non-zero on error.
int job_queue_init(struct job_queue *job_queue, int capacity);

// Destroy the job queue.  Blocks until the queue is empty before it
// is destroyed.
int job_queue_destroy(struct job_queue *job_queue);

// Push an element onto the end of the job queue.  Blocks if the
// job_queue is full (its size is equal to its capacity).  Returns
// non-zero on error.  It is an error to push a job onto a queue that
// has been destroyed.
int job_queue_push(struct job_queue *job_queue, void *data);

// Pop an element from the front of the job queue.  Blocks if the
// job_queue contains zero elements.  Returns non-zero on error.  If
// job_queue_destroy() has been called (possibly after the call to
// job_queue_pop() blocked), this function will return -1.
int job_queue_pop(struct job_queue *job_queue, void **data);

#endif
