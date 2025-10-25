// Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <err.h>
#include <pthread.h>
#include "job_queue.h"

// Initialize print mutex
pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;

static int fauxgrep_file_mt(char const *needle, char const *path) {
  // Open file
  FILE *f = fopen(path, "r");
  // If file fails to open, return with warning
  if (f == NULL) {
    warn("failed to open %s", path);
    return -1;
  }

  char *line = NULL;
  size_t linelen = 0;
  int linenum = 1;

  while (getline(&line, &linelen, f) != -1) {
    // If the substring is found in the haystack, print where it was found
    if (strstr(line, needle) != NULL) {
      assert(pthread_mutex_lock(&stdout_mutex) == 0);
      printf("%s:%d: %s", path, linenum, line);
      assert(pthread_mutex_unlock(&stdout_mutex) == 0);
    }
    linenum++;
  }
  // Free memory for loaded line, close file, and return
  free(line);
  fclose(f);
  return 0;
}

// -- Instruction set for worker threads --
static void* worker(void *arg){
  struct worker* w = (struct worker*)arg;
  struct job_queue *jq = w->jq;
  const char *needle = w->needle;

  for (;;) { // endless for-loop/no condtion loop
    char *path = NULL;
    if (job_queue_pop(jq, (void**)&path) != 0 || path == NULL) { // Pop job of queue
      break; // pop failed/queue error or shutdown
    }
    // Process the file popped from the queue
    (void)fauxgrep_file_mt(needle, path); // Casting void ensures we get the side-effects of the function but disregarding the return value
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
        // strdup: FTS uses internal buffers that get reused, so we must copy
        char *copy = strdup(p->fts_path);
        if (job_queue_push(jq, copy) != 0) {
          warn("job_queue_push() failed - stopping traversal"); // If push fails due to shutdown, stop producing.
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
    err(1, "usage: [-n INT] STRING paths...");
    exit(1);
  }

  int num_threads = 1;
  char const *needle;
  char * const *paths;

  if (argc > 3 && strcmp(argv[1], "-n") == 0) {
    // Simple atoi parsing (same note as template): non-numeric becomes 0.
    num_threads = atoi(argv[2]);
    if (num_threads < 1) {
      err(1, "invalid thread count: %s", argv[2]);
    }
    needle = argv[3];
    paths = &argv[4];
  } else {
    needle = argv[1];
    paths = &argv[2];
  }
  // Initialize the job queue
  struct job_queue jq;
  if (job_queue_init(&jq, 128) != 0) {
    err(1, "job_queue_init() failed");
  }

  struct worker w = {
    .jq = &jq,
    .needle = needle
  };
  // Allocate for worker threads
  pthread_t *threads = calloc((size_t)num_threads, sizeof(pthread_t));
  if (!threads) { 
    err(1, "calloc() for threads failed");
  }
  // Create worker threads
  for (int i = 0; i < num_threads; i++){
    if (pthread_create(&threads[i], NULL, worker, &w) != 0 ){
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
  for (int i = 0; i < num_threads; i++){
    if (pthread_join(threads[i], NULL) != 0) {
      err(1, "pthread_join() failed");
    }
  }
  free(threads);
  // Shut down the job queue
  job_queue_destroy(&jq);

  return 0;
}