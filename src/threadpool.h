#pragma once

typedef struct tpool *tpool_t;
typedef struct future *future_t;

/**
 * Create a pool with num_threads worker threads.
 * Returns NULL if num_threads is 0 or resources can't be allocated.
 */
tpool_t tpool_create(unsigned int num_threads);

/**
 * Schedule f(pool, arg) to run on a worker thread. Tasks may schedule
 * further tasks through the pool they receive.
 * Returns 0 on success, -1 on allocation failure or if the pool is shutting down.
 */
int tpool_schedule_task(tpool_t pool, void (*f)(tpool_t, void *), void *arg);

/**
 * Schedule f(pool, arg) to run on a worker thread and return a future for
 * its result. Returns immediately; use future_get to wait for the result.
 * Returns NULL on allocation failure or if the pool is shutting down.
 */
future_t tpool_submit(tpool_t pool, void *(*f)(tpool_t, void *), void *arg);

/**
 * Block until the future's task has finished and return its result.
 * Safe to call from inside a task: if the task hasn't started yet, the
 * caller runs it itself instead of waiting. May be called more than once.
 */
void *future_get(future_t fut);

/**
 * Wait for the future's task to finish (if it hasn't) and free the future.
 * Every future must be freed before its pool is destroyed.
 */
void future_free(future_t fut);

/**
 * Block until every scheduled task (including ones scheduled by tasks)
 * has finished. Must not be called from inside a task.
 */
void tpool_join(tpool_t pool);

/**
 * Finish all scheduled tasks, stop the worker threads and free the pool.
 * Must not be called from inside a task.
 */
void tpool_destroy(tpool_t pool);
