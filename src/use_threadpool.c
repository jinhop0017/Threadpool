#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "threadpool.h"

void my_task(tpool_t pool, void *arg) {
  (void) pool;  // unused; tasks could schedule more work through it
  long i = (long) arg;
  printf("Task %ld started\n", i);
  usleep(1000000);  // Simulate a running time for a function
  printf("Task %ld ended\n", i);
}

/* Parse a positive integer argument, or return -1 if it isn't one. */
static long parse_positive(const char *s) {
  char *end;
  long n = strtol(s, &end, 10);
  if (*s == '\0' || *end != '\0' || n <= 0)
    return -1;
  return n;
}

int main(int argc, char *argv[]) {
  tpool_t pool;
  long num_threads, num_tasks;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s NUM_THREADS NUM_TASKS\n", argv[0]);
    return 1;
  }

  num_threads = parse_positive(argv[1]);
  num_tasks = parse_positive(argv[2]);
  if (num_threads < 0 || num_tasks < 0) {
    fprintf(stderr, "NUM_THREADS and NUM_TASKS must be positive integers\n");
    return 1;
  }

  pool = tpool_create(num_threads);
  if (pool == NULL) {
    fprintf(stderr, "Failed to create thread pool\n");
    return 1;
  }

  for (long i = 0; i < num_tasks; i++) {
    if (tpool_schedule_task(pool, my_task, (void *) i) != 0) {
      fprintf(stderr, "Failed to schedule task %ld\n", i);
      break;
    }
  }

  tpool_join(pool);
  tpool_destroy(pool);

  return 0;
}
