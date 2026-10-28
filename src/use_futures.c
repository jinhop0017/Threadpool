#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "threadpool.h"

#define CUTOFF 100000  // ranges smaller than this are summed directly

typedef struct {
  long lo, hi;
} range_t;

/*
 * Sum lo..hi-1. Splits the range in half, submits the left half as its own
 * task, sums the right half on this thread, then adds the two results.
 */
void *sum_range(tpool_t pool, void *arg) {
  range_t *r = arg;

  if (r->hi - r->lo <= CUTOFF) {
    long sum = 0;
    for (long i = r->lo; i < r->hi; i++)
      sum += i;
    return (void *) (intptr_t) sum;
  }

  long mid = r->lo + (r->hi - r->lo) / 2;
  range_t left = { r->lo, mid };
  range_t right = { mid, r->hi };

  future_t f = tpool_submit(pool, sum_range, &left);
  long right_sum = (intptr_t) sum_range(pool, &right);
  long left_sum = (intptr_t) (f ? future_get(f) : sum_range(pool, &left));
  future_free(f);

  return (void *) (intptr_t) (left_sum + right_sum);
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
  if (argc != 3) {
    fprintf(stderr, "Usage: %s NUM_THREADS N\n", argv[0]);
    return 1;
  }

  long num_threads = parse_positive(argv[1]);
  long n = parse_positive(argv[2]);
  if (num_threads < 0 || n < 0) {
    fprintf(stderr, "NUM_THREADS and N must be positive integers\n");
    return 1;
  }

  tpool_t pool = tpool_create(num_threads);
  if (pool == NULL) {
    fprintf(stderr, "Failed to create thread pool\n");
    return 1;
  }

  range_t all = { 1, n + 1 };
  future_t f = tpool_submit(pool, sum_range, &all);
  if (f == NULL) {
    fprintf(stderr, "Failed to submit task\n");
    tpool_destroy(pool);
    return 1;
  }
  long sum = (intptr_t) future_get(f);
  future_free(f);
  tpool_destroy(pool);

  long expected = n * (n + 1) / 2;
  printf("sum(1..%ld) = %ld (expected %ld) %s\n",
         n, sum, expected, sum == expected ? "OK" : "WRONG");
  return sum == expected ? 0 : 1;
}
