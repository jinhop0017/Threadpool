#include <stdlib.h>
#include <pthread.h>

#include "threadpool.h"

typedef struct task {
    void (*function)(tpool_t, void *);
    void *arg;
    struct task *next;
} task_t;

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

        // Unlock before executing task to allow other threads to access task queue
        pthread_mutex_unlock(&pool->lock);
        task->function(pool, task->arg);
        free(task);
        pthread_mutex_lock(&pool->lock);

        // Wake joiners once the last outstanding task finishes
        if (--pool->pending == 0) {
            pthread_cond_broadcast(&pool->all_done);
        }
    }
    pthread_mutex_unlock(&pool->lock);

    return NULL;
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
    task->next = NULL;

    pthread_mutex_lock(&pool->lock);
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        free(task);
        return -1;
    }

    // Append to the back of the queue
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

    for (unsigned int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->task_available);
    pthread_cond_destroy(&pool->all_done);
    free(pool->threads);
    free(pool);
}
