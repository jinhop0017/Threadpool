#pragma once

typedef struct tpool *tpool_t;

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
 * Block until every scheduled task (including ones scheduled by tasks)
 * has finished. Must not be called from inside a task.
 */
void tpool_join(tpool_t pool);

/**
 * Finish all scheduled tasks, stop the worker threads and free the pool.
 * Must not be called from inside a task.
 */
void tpool_destroy(tpool_t pool);
