#include <stdlib.h>
#include <pthread.h>

#include "threadpool.h"

/*
 * A queue node. Plain tasks set function; future tasks set future.
 * A node with neither is a future task that future_get already claimed
 * and ran; workers just discard it.
 */
typedef struct task {
    void (*function)(tpool_t, void *);
    void *arg;
    future_t future;
    struct task *next;
} task_t;

enum future_state { FUT_PENDING, FUT_RUNNING, FUT_DONE };

struct future {
    tpool_t pool;
    void *(*function)(tpool_t, void *);
    void *arg;
    void *result;
    enum future_state state;    // protected by pool->lock
    task_t *task;               // queue node while FUT_PENDING
    pthread_cond_t done;        // signaled when state becomes FUT_DONE
};

struct tpool {
    unsigned int num_threads;
    pthread_t *threads;
    task_t *head;               // next task to run
    task_t *tail;               // last task, for O(1) enqueue
    pthread_mutex_t lock;       // protects every field below
    pthread_cond_t task_available;
    pthread_cond_t all_done;
    unsigned long pending;      // tasks queued or currently running
    int shutdown;
};

/**
 * Record that a task finished. Called with pool->lock held.
 */
static void task_done_locked(tpool_t pool) {
    // Wake joiners once the last outstanding task finishes
    if (--pool->pending == 0) {
        pthread_cond_broadcast(&pool->all_done);
    }
}

/**
 * Run a pending future on the calling thread and publish its result.
 * Called with pool->lock held; returns with it held.
 */
static void run_future_locked(tpool_t pool, future_t fut) {
    fut->state = FUT_RUNNING;
    fut->task = NULL;

    pthread_mutex_unlock(&pool->lock);
    void *result = fut->function(pool, fut->arg);
    pthread_mutex_lock(&pool->lock);

    fut->result = result;
    fut->state = FUT_DONE;
    pthread_cond_broadcast(&fut->done);
    task_done_locked(pool);
}

/**
 * Base procedure of every worker thread.  Calls available tasks
 * or blocks until a task becomes available or the pool shuts down.
 */
static void *worker_thread(void *pool_v) {
    tpool_t pool = pool_v;

    pthread_mutex_lock(&pool->lock);
    while (1) {
        while (pool->head == NULL && !pool->shutdown) {
            pthread_cond_wait(&pool->task_available, &pool->lock);
        }
        if (pool->head == NULL) {
            // Shutting down and nothing left to run
            break;
        }

        // Take the task from the front of the queue
        task_t *task = pool->head;
        pool->head = task->next;
        if (pool->head == NULL) {
            pool->tail = NULL;
        }
        void (*function)(tpool_t, void *) = task->function;
        void *arg = task->arg;
        future_t fut = task->future;
        free(task);

        if (fut != NULL) {
            run_future_locked(pool, fut);
        } else if (function != NULL) {
            // Unlock before executing task to allow other threads to access task queue
            pthread_mutex_unlock(&pool->lock);
            function(pool, arg);
            pthread_mutex_lock(&pool->lock);
            task_done_locked(pool);
        }
        // Otherwise future_get already ran it and counted it as done
    }
    pthread_mutex_unlock(&pool->lock);

    return NULL;
}

/**
 * Append a task to the queue. Returns -1 (and leaves task unqueued)
 * if the pool is shutting down.
 */
static int enqueue_task(tpool_t pool, task_t *task) {
    pthread_mutex_lock(&pool->lock);
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }

    if (pool->tail == NULL) {
        pool->head = task;
    } else {
        pool->tail->next = task;
    }
    pool->tail = task;
    pool->pending++;

    pthread_cond_signal(&pool->task_available);
    pthread_mutex_unlock(&pool->lock);
    return 0;
}

/**
 * Create a new thread pool with num_threads worker threads.
 */
tpool_t tpool_create(unsigned int num_threads) {
    if (num_threads == 0) {
        return NULL;
    }

    tpool_t pool = calloc(1, sizeof(*pool));
    if (pool == NULL) {
        return NULL;
    }
    pool->threads = calloc(num_threads, sizeof(pthread_t));
    if (pool->threads == NULL) {
        free(pool);
        return NULL;
    }

    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->task_available, NULL);
    pthread_cond_init(&pool->all_done, NULL);

    for (unsigned int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            // Stop and join the threads that did start
            tpool_destroy(pool);
            return NULL;
        }
        pool->num_threads++;
    }

    return pool;
}

/**
 * Schedule task f(pool, arg) to be executed.
 */
int tpool_schedule_task(tpool_t pool, void (*f)(tpool_t, void *), void *arg) {
    task_t *task = malloc(sizeof(*task));
    if (task == NULL) {
        return -1;
    }
    task->function = f;
    task->arg = arg;
    task->future = NULL;
    task->next = NULL;

    if (enqueue_task(pool, task) != 0) {
        free(task);
        return -1;
    }
    return 0;
}

/**
 * Schedule f(pool, arg) and return a future for its result.
 */
future_t tpool_submit(tpool_t pool, void *(*f)(tpool_t, void *), void *arg) {
    future_t fut = malloc(sizeof(*fut));
    task_t *task = malloc(sizeof(*task));
    if (fut == NULL || task == NULL) {
        free(fut);
        free(task);
        return NULL;
    }

    fut->pool = pool;
    fut->function = f;
    fut->arg = arg;
    fut->result = NULL;
    fut->state = FUT_PENDING;
    fut->task = task;
    pthread_cond_init(&fut->done, NULL);

    task->function = NULL;
    task->arg = NULL;
    task->future = fut;
    task->next = NULL;

    if (enqueue_task(pool, task) != 0) {
        pthread_cond_destroy(&fut->done);
        free(fut);
        free(task);
        return NULL;
    }
    return fut;
}

/**
 * Wait for the future's task to finish and return its result.
 */
void *future_get(future_t fut) {
    tpool_t pool = fut->pool;

    pthread_mutex_lock(&pool->lock);
    if (fut->state == FUT_PENDING) {
        // Not started yet: run it here rather than wait. A task waiting on
        // its own subtasks would otherwise deadlock once every worker is
        // blocked in future_get. Unlink the node so the worker that later
        // dequeues it skips it and never touches this future.
        fut->task->future = NULL;
        run_future_locked(pool, fut);
    }
    while (fut->state != FUT_DONE) {
        pthread_cond_wait(&fut->done, &pool->lock);
    }
    void *result = fut->result;
    pthread_mutex_unlock(&pool->lock);

    return result;
}

/**
 * Wait for the future's task to finish and release the future.
 */
void future_free(future_t fut) {
    if (fut == NULL) {
        return;
    }
    future_get(fut);
    pthread_cond_destroy(&fut->done);
    free(fut);
}

/**
 * Wait (by blocking) until all tasks have completed and thread pool is thus idle
 */
void tpool_join(tpool_t pool) {
    pthread_mutex_lock(&pool->lock);
    while (pool->pending > 0) {
        pthread_cond_wait(&pool->all_done, &pool->lock);
    }
    pthread_mutex_unlock(&pool->lock);
}

/**
 * Finish outstanding tasks, stop all worker threads and release the pool.
 */
void tpool_destroy(tpool_t pool) {
    if (pool == NULL) {
        return;
    }

    tpool_join(pool);

    pthread_mutex_lock(&pool->lock);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->task_available);
    pthread_mutex_unlock(&pool->lock);

    // Workers drain any leftover claimed nodes before exiting
    for (unsigned int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->task_available);
    pthread_cond_destroy(&pool->all_done);
    free(pool->threads);
    free(pool);
}
