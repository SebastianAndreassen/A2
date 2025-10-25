// Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <err.h>
#include "job_queue.h"
#include "histogram.h"  

static int global_histogram[8] = {0};
static pthread_mutex_t hist_mutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

#define PRINT_INTERVAL 100000   // bytes per progress update 

// Merge local -> global under lock
static void merge_into_global_mt(int local[8]) {
  pthread_mutex_lock(&hist_mutex);
  merge_histogram(local, global_histogram); 
  pthread_mutex_unlock(&hist_mutex);
}

// Print current global safely
static void print_global_mt(void) {
  int snap[8];

  // Take a snapshot under the histogram lock
  pthread_mutex_lock(&hist_mutex);
  memcpy(snap, global_histogram, sizeof snap);
  pthread_mutex_unlock(&hist_mutex);

  // Print the snapshot under the print lock
  pthread_mutex_lock(&print_mutex);
  print_histogram(snap);
  pthread_mutex_unlock(&print_mutex);
}

// -- Instruction set for worker threads --
static void* worker(void *arg) {
  struct worker *wa = arg;
  struct job_queue *jq = wa->jq;

  for (;;) {
    char *path = NULL;
    if (job_queue_pop(jq, (void**)&path) != 0 || path == NULL) { // Pop job of the queue
      break; // pop failed/queue error or shutdown
    }

    // Try open file in binary
    FILE *f = fopen(path, "rb");
    if (!f) {
      fflush(stdout);
      warn("failed to open %s", path);
      free(path);
      continue;
    }

    int local[8] = {0};
    size_t bytes_since_print = 0;
    unsigned char byte;

    // Read byte-by-byte since update_histogram() takes ONE byte 
    while (fread(&byte, 1, 1, f) == 1) {
      update_histogram(local, byte);
      if (++bytes_since_print % PRINT_INTERVAL == 0) {
        merge_into_global_mt(local);
        print_global_mt();
        bytes_since_print = 0;
      }
    }
    fclose(f);

    // Flush remainder for this file and show progress
    merge_into_global_mt(local);
    print_global_mt();

    free(path);
  }

  return NULL;
}

// Producer that traverse directories with FTS and enqueue files
static void traverse_and_enqueue(struct job_queue *jq, char * const *paths) {
  int fts_options = FTS_LOGICAL | FTS_NOCHDIR;
  FTS *ftsp;
  if ((ftsp = fts_open(paths, fts_options, NULL)) == NULL) {
    err(1, "fts_open() failed");
  }

  FTSENT *p;
  while ((p = fts_read(ftsp)) != NULL) {
    switch (p->fts_info) {
      case FTS_D:
        break;
      case FTS_F: {
        char *copy = strdup(p->fts_path);
        // strdup() because FTS reuses internal buffers
        if (job_queue_push(jq, copy) != 0) {
          // queue shutting down; stop producing
          warn("job_queue_push() failed - stopping traversal");
          free(copy);
          break;
        }
      }
      default:
        break;
    }
  }
  fts_close(ftsp);
}

int main(int argc, char * const *argv) {
  if (argc < 2) {
    err(1, "usage: paths...");
    exit(1);
  }

  int num_threads = 1;
  char * const *paths;

  // Parse optional "-n k" like the template 
  if (argc > 3 && strcmp(argv[1], "-n") == 0) {
    num_threads = atoi(argv[2]);
    if (num_threads < 1) err(1, "invalid thread count: %s", argv[2]);
    paths = &argv[3];
  } else {
    paths = &argv[1];
  }

  // Initialize job queue
  struct job_queue jq;
  if (job_queue_init(&jq, 128) != 0) {
    err(1, "job_queue_init() failed");
  }

  // Allocate memory for threads
  pthread_t *threads = calloc((size_t)num_threads, sizeof(pthread_t));
  if (!threads) {
    err(1, "calloc() for threads failed");
  }

  struct worker wa = { 
    .jq = &jq 
  };
  // Create worker threads 
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&threads[i], NULL, worker, &wa) != 0) {
      err(1, "pthread_create() failed");
    }
  }
  // Traverse directories and enqueue jobs
  traverse_and_enqueue(&jq, paths);
  // Producer done – signal workers to stop
  for (int i = 0; i < num_threads; i++) {
      job_queue_push(&jq, NULL);
  }
  // Join all threads
  for (int i = 0; i < num_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      err(1, "pthread_join failed");
    }
  }
  free(threads);
  // Shut down the job queue
  job_queue_destroy(&jq);
  // Final tidy output position just like the ST version
  move_lines(9); // keep UI neat after last print  

  return 0;
}