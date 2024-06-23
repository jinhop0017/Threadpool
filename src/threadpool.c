#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

typedef struct task {
    void (*function)(void *, void *);
    void *arg;
    struct task *next;
} task_t;

typedef struct tpool {
    unsigned int max_threads;
    unsigned int num_threads;
    task_t *task_queue;
    pthread_mutex_t lock;
    pthread_cond_t task_available;
    pthread_cond_t task_completed;
    volatile int tasks_to_finish;
    volatile int tasks_completed;
} *tpool_t;

/**
 * Base procedure of every worker thread.  Calls available tasks
 * or blocks until a task becomes available.
 */
void *worker_thread(void *pool_v) {
    tpool_t pool = (tpool_t)pool_v;

    while (1) {
        pthread_mutex_lock(&pool->lock);
        while (pool->task_queue == NULL) {
            // Wait for a task to be available
            pthread_cond_wait(&pool->task_available, &pool->lock);
        }

        // Get the task from the queue
        task_t *task = pool->task_queue;
        pool->task_queue = task->next;

        // Unlock before executing task to allow other threads to access task queue
        pthread_mutex_unlock(&pool->lock);

        // Execute the task
        task->function(pool, task->arg);

        // Free memory allocated for the task
        free(task);

        // Signal task completion
        pthread_mutex_lock(&pool->lock);
        pool->tasks_completed += 1;
        pthread_cond_signal(&pool->task_completed);
        pthread_mutex_unlock(&pool->lock);
    }

    return NULL;
}

/**
 * Create a new thread pool with max_threads thread-count limit.
 */
tpool_t tpool_create(unsigned int max_threads) {
    tpool_t pool = (tpool_t)malloc(sizeof(struct tpool));

    pool->max_threads = max_threads;
    pool->num_threads = 0;
    pool->task_queue = NULL;
    pool->tasks_completed = 0;
    pool->tasks_to_finish = 0;

    // Initialize mutex and condition variables
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->task_available, NULL);
    pthread_cond_init(&pool->task_completed, NULL);

    // Create worker threads
    for (unsigned int i = 0; i < max_threads; i++) {
        pthread_t thread;
        pthread_create(&thread, NULL, worker_thread, pool);
        pool->num_threads++;
    }

    return pool;
}

/**
 * Schedule task f(arg) to be executed.
 */
void tpool_schedule_task(tpool_t pool, void (*f)(void *, void *), void *arg) {
    // Create a new task
    pool->tasks_to_finish += 1;
    task_t *task = (task_t *)malloc(sizeof(task_t));

    task->function = f;
    task->arg = arg;
    task->next = NULL;

    pthread_mutex_lock(&pool->lock);
    // Add the task to the queue
    if (pool->task_queue == NULL) {
        pool->task_queue = task;
    } else {
        task_t *curr = pool->task_queue;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = task;
    }
    // Signal that a task is available
    pthread_cond_signal(&pool->task_available);
    pthread_mutex_unlock(&pool->lock);
}

/**
 * Wait (by blocking) until all tasks have completed and thread pool is thus idle
 */
void tpool_join(tpool_t pool) {
    pthread_mutex_lock(&pool->lock);
    while (pool->task_queue != NULL || (pool->tasks_completed != pool->tasks_to_finish)) {
        // Wait for all tasks to complete
        pthread_cond_wait(&pool->task_completed, &pool->lock);
    }
    pthread_mutex_unlock(&pool->lock);
}