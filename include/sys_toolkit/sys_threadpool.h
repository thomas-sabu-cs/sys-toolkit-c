/**
 * @file sys_threadpool.h
 * @brief Lightweight pthread-based thread pool for structured concurrency.
 *
 * The goal of this module is not to be feature-complete, but to demonstrate
 * disciplined use of pthread primitives under strict compilation flags.
 * The design focuses on:
 * - Bounded work queues with back-pressure.
 * - Predictable, cooperative shutdown semantics.
 * - Simplicity: a small, auditable surface area for systems interviews.
 */

#ifndef SYS_THREADPOOL_H
#define SYS_THREADPOOL_H

#include <pthread.h>
#include <stddef.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sys_threadpool_task_fn)(void *arg);

/**
 * @brief Opaque thread pool type.
 *
 * The internal layout is intentionally exposed as a struct to make the
 * implementation easy to reason about in systems contexts, but users
 * should treat it as opaque and only manipulate it via the API.
 */
typedef struct sys_threadpool {
    pthread_t *threads;
    size_t thread_count;

    sys_threadpool_task_fn *tasks;
    void **task_args;
    size_t queue_capacity;
    size_t head;
    size_t tail;
    size_t count;

    _Atomic size_t pending_tasks;

    pthread_mutex_t mutex;
    pthread_cond_t cond_not_empty;
    pthread_cond_t cond_not_full;

    int shutdown;
} sys_threadpool;

/**
 * @brief Initialize a thread pool with the given number of workers.
 *
 * @param pool Pointer to the pool structure to initialize.
 * @param thread_count Number of worker threads to spawn.
 * @param queue_capacity Maximum number of enqueued tasks.
 * @return 0 on success, non-zero on failure (no partial initialization leaks).
 */
int sys_threadpool_init(sys_threadpool *pool, size_t thread_count, size_t queue_capacity);

/**
 * @brief Enqueue a task for execution.
 *
 * This function blocks if the internal queue is full, providing natural
 * back-pressure under sustained load. It is safe to call from multiple
 * producer threads.
 *
 * @param pool Pointer to an initialized pool.
 * @param fn Task function pointer.
 * @param arg User data passed to the task function.
 * @return 0 on success, non-zero if the pool is shutting down or arguments
 *         are invalid.
 */
int sys_threadpool_submit(sys_threadpool *pool, sys_threadpool_task_fn fn, void *arg);

/**
 * @brief Gracefully shut down the thread pool and join all workers.
 *
 * No new tasks are accepted after shutdown begins. Pending tasks in the
 * queue are drained before the workers exit.
 *
 * @param pool Pointer to an initialized pool.
 */
void sys_threadpool_shutdown(sys_threadpool *pool);

#ifdef __cplusplus
}
#endif

#endif /* SYS_THREADPOOL_H */

