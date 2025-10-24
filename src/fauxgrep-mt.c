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

// err.h contains various nonstandard BSD extensions, but they are
// very handy.
#include <err.h>

#include <pthread.h>

#include "job_queue.h"
struct tworker{
  char const *needle;
  pthread_t id_t;
  struct job_queue *jq;
};
pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;


int fauxgrep_file(char const *needle, char const *path) {
  FILE *f = fopen(path, "r");

  if (f == NULL) {
    warn("failed to open %s", path);
    return -1;
  }

  char *line = NULL;
  size_t linelen = 0;
  int lineno = 1;

  while (getline(&line, &linelen, f) != -1) {
    // FIX #2: only lock when we actually print
    if (strstr(line, needle) != NULL) {
      assert(pthread_mutex_lock(&stdout_mutex) == 0);
      printf("%s:%d: %s", path, lineno, line);
      assert(pthread_mutex_unlock(&stdout_mutex) == 0);
    }
    lineno++;
  }

  free(line);
  fclose(f);

  return 0;
}

void* worker(void *arg){
  struct tworker* tw =(struct tworker*)arg;
  void *data;

  while(job_queue_pop(tw->jq,&data)==0){
    char *path=(char*) data;
    fauxgrep_file(tw->needle,path);
    free(path);
  }
  return NULL;
}

int main(int argc, char * const *argv) {
  if (argc < 2) {
    err(1, "usage: [-n INT] STRING paths...");
    exit(1);
  }

  int num_threads = 1;
  char const *needle = argv[1];
  char * const *paths = &argv[2];


  if (argc > 3 && strcmp(argv[1], "-n") == 0) {
    // Since atoi() simply returns zero on syntax errors, we cannot
    // distinguish between the user entering a zero, or some
    // non-numeric garbage.  In fact, we cannot even tell whether the
    // given option is suffixed by garbage, i.e. '123foo' returns
    // '123'.  A more robust solution would use strtol(), but its
    // interface is more complicated, so here we are.
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

  // Initialise the job queue and some worker threads here.
  struct job_queue *jq;
  jq=malloc(sizeof(struct job_queue));
  job_queue_init(jq,10);

  // (Optional earlier push of argv paths removed — not needed)

  struct tworker tw[num_threads];
  for(int i=0;i<num_threads;i++){
    tw[i].jq=jq;
    tw[i].needle=needle;
    if(pthread_create(&tw[i].id_t,NULL,worker,&tw[i])!=0 ){
      printf("Initizilization failde");
      return EXIT_FAILURE;
    }
  }
  // FTS_LOGICAL = follow symbolic links
  // FTS_NOCHDIR = do not change the working directory of the process
  //
  // (These are not particularly important distinctions for our simple
  // uses.)
  int fts_options = FTS_LOGICAL | FTS_NOCHDIR;

  FTS *ftsp;
  if ((ftsp = fts_open(paths, fts_options, NULL)) == NULL) {
    err(1, "fts_open() failed");
    return -1;
  }

  FTSENT *p;
  while ((p = fts_read(ftsp)) != NULL) {
    switch (p->fts_info) {
    case FTS_D:
      break;
    case FTS_F:
  

      {
        char *copy = strdup(p->fts_path);
        if (!copy) err(1, "strdup failed");
        job_queue_push(jq, copy);
      }
      break;
    default:
      break;
    }
  }

  fts_close(ftsp);
  // Shut down the job queue and the worker threads here.
  
  job_queue_finished(jq);

  for(int i=0;i<num_threads;i++){
    pthread_join(tw[i].id_t,NULL);
  }

  job_queue_destroy(jq);
  
  free(jq);

  return 0;
}
