# Thread Pool in C

A small thread pool library in C using pthreads. It runs tasks on a fixed set of worker threads, and supports **futures** so tasks can return results, including tasks that split their work into subtasks and wait on them (fork-join).

## Features

- **Fixed pool of worker threads** that are reused across tasks instead of creating a thread per task
- **Fire-and-forget tasks** with `tpool_schedule_task`, and `tpool_join` to wait for all of them
- **Futures** with `tpool_submit` / `future_get`, so you get a task's result and wait for that task only
- **Recursive (fork-join) tasks**: tasks receive the pool and can submit subtasks. `future_get` runs a not-yet-started task on the calling thread instead of blocking, so recursive work can't deadlock the pool, even with a single thread
- **Clean shutdown** with `tpool_destroy`, which finishes queued work, stops the threads and frees everything
- **FIFO task queue** with O(1) enqueue, protected by a single mutex

## Building

Requires a C compiler (gcc or clang), make, and pthreads (built into macOS and Linux).

```bash
make          # builds bin/use_threadpool and bin/use_futures
make clean    # removes build outputs
```

## Running the demos

**Plain tasks:** schedules `NUM_TASKS` tasks that each sleep for a second, across `NUM_THREADS` threads.

```bash
./bin/use_threadpool NUM_THREADS NUM_TASKS
./bin/use_threadpool 4 10
```

**Futures:** computes 1 + 2 + … + N with a recursive parallel sum, and checks the result against n(n+1)/2.

```bash
./bin/use_futures NUM_THREADS N
./bin/use_futures 4 100000000
# sum(1..100000000) = 5000000050000000 (expected 5000000050000000) OK
```

## Using the library

Copy `src/threadpool.h` and `src/threadpool.c` into your project, and compile with `-pthread`:

```bash
gcc -Wall -pthread myprog.c threadpool.c -o myprog
```

### 1. Create a pool

```c
#include "threadpool.h"

tpool_t pool = tpool_create(4);   // 4 worker threads
```

The workers start immediately and wait for tasks. A good default is the number of CPU cores. Returns `NULL` on failure.

### 2. Write a task function

Every task takes the pool and one `void *` argument:

```c
// No result: use with tpool_schedule_task
void print_task(tpool_t pool, void *arg);

// Returns a result: use with tpool_submit
void *square(tpool_t pool, void *arg) {
    long n = (long) arg;
    return (void *) (n * n);
}
```

The `pool` parameter lets a task submit more tasks. To pass several values, put them in a struct and pass a pointer to it (see `range_t` in `src/use_futures.c`).

### 3. Submit tasks

```c
// Fire and forget
tpool_schedule_task(pool, print_task, arg);   // returns 0, or -1 on failure

// With a result
future_t f = tpool_submit(pool, square, (void *) 7);   // returns NULL on failure
```

Both return immediately. The task runs as soon as a worker is free.

### 4. Wait for results

```c
tpool_join(pool);                    // wait for every scheduled task

long r = (long) future_get(f);       // wait for this one task → 49
```

### 5. Clean up

```c
future_free(f);         // once per future, when you're done with its result
tpool_destroy(pool);    // once, at the end: finishes queued tasks and stops the threads
```

### Example: recursive parallel sum

Each task splits its range in half, submits the left half as a subtask, sums the right half itself, then waits for the left half's result. This is `src/use_futures.c`, simplified:

```c
typedef struct { long lo, hi; } range_t;

void *sum_range(tpool_t pool, void *arg) {
    range_t *r = arg;

    if (r->hi - r->lo <= CUTOFF) {               // small enough: just add
        long sum = 0;
        for (long i = r->lo; i < r->hi; i++)
            sum += i;
        return (void *) sum;
    }

    long mid = r->lo + (r->hi - r->lo) / 2;
    range_t left = { r->lo, mid }, right = { mid, r->hi };

    future_t f = tpool_submit(pool, sum_range, &left);   // left half: another thread
    long right_sum = (long) sum_range(pool, &right);     // right half: this thread
    long left_sum  = (long) future_get(f);               // wait for the left half
    future_free(f);

    return (void *) (left_sum + right_sum);
}
```

`CUTOFF` keeps tasks large enough that the work outweighs the cost of scheduling them.

## API reference

| Function | Description |
|---|---|
| `tpool_t tpool_create(unsigned int num_threads)` | Create a pool with `num_threads` workers. Returns `NULL` if `num_threads` is 0 or on allocation failure. |
| `int tpool_schedule_task(tpool_t pool, void (*f)(tpool_t, void *), void *arg)` | Schedule `f(pool, arg)`. Returns 0, or -1 on failure or after shutdown. |
| `future_t tpool_submit(tpool_t pool, void *(*f)(tpool_t, void *), void *arg)` | Schedule `f(pool, arg)` and return a future for its result. Returns `NULL` on failure or after shutdown. |
| `void *future_get(future_t fut)` | Wait for the task and return its result. Can be called more than once, and from inside tasks. |
| `void future_free(future_t fut)` | Wait for the task if needed, then free the future. |
| `void tpool_join(tpool_t pool)` | Wait until every scheduled task, including subtasks, has finished. |
| `void tpool_destroy(tpool_t pool)` | Finish queued tasks, stop the workers and free the pool. |

## Rules

1. **Don't call `tpool_join` or `tpool_destroy` from inside a task.** It waits for itself and never returns. `future_get` inside a task is fine.
2. **Free every future before calling `tpool_destroy`.**
3. **Keep `arg` valid until the task finishes.** Don't pass a pointer to a local variable from a function that returns before the task runs; use `malloc`, or wait for the task before returning.
4. **Protect shared data yourself.** The pool keeps its own internals thread-safe, not the data your tasks share; use a mutex or atomics.

## Project structure

```
src/
  threadpool.h      Public API
  threadpool.c      Implementation
  use_threadpool.c  Demo: plain tasks
  use_futures.c     Demo: recursive parallel sum with futures
Makefile            Builds both demos into bin/
```

## Contributing

Contributions are welcome! Fork the repository and open a pull request with your improvements.
