#include "sys_toolkit/sys_threadpool.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void *sys_threadpool_worker(void *arg)
{
    sys_threadpool *pool = (sys_threadpool *)arg;

    for (;;) {
        sys_threadpool_task_fn task = NULL;
        void *task_arg = NULL;

        if (pthread_mutex_lock(&pool->mutex) != 0) {
            break;
        }

        while (pool->count == 0U && !pool->shutdown) {
            if (pthread_cond_wait(&pool->cond_not_empty, &pool->mutex) != 0) {
                (void)pthread_mutex_unlock(&pool->mutex);
                return NULL;
            }
        }

        if (pool->shutdown && pool->count == 0U) {
            (void)pthread_mutex_unlock(&pool->mutex);
            break;
        }

        task = pool->tasks[pool->head];
        task_arg = pool->task_args[pool->head];
        pool->head = (pool->head + 1U) % pool->queue_capacity;
        pool->count--;

        (void)pthread_cond_signal(&pool->cond_not_full);
        (void)pthread_mutex_unlock(&pool->mutex);

        if (task != NULL) {
            task(task_arg);
        }
    }

    return NULL;
}

int sys_threadpool_init(sys_threadpool *pool, size_t thread_count, size_t queue_capacity)
{
    if (pool == NULL || thread_count == 0U || queue_capacity == 0U) {
        return -1;
    }

    memset(pool, 0, sizeof(*pool));

    pool->threads = (pthread_t *)calloc(thread_count, sizeof(pthread_t));
    pool->tasks = (sys_threadpool_task_fn *)calloc(queue_capacity, sizeof(sys_threadpool_task_fn));
    pool->task_args = (void **)calloc(queue_capacity, sizeof(void *));
    if (pool->threads == NULL || pool->tasks == NULL || pool->task_args == NULL) {
        free(pool->threads);
        free(pool->tasks);
        free(pool->task_args);
        return -1;
    }

    pool->thread_count = thread_count;
    pool->queue_capacity = queue_capacity;
    pool->head = 0U;
    pool->tail = 0U;
    pool->count = 0U;
    pool->shutdown = 0;

    if (pthread_mutex_init(&pool->mutex, NULL) != 0 ||
        pthread_cond_init(&pool->cond_not_empty, NULL) != 0 ||
        pthread_cond_init(&pool->cond_not_full, NULL) != 0) {
        (void)pthread_mutex_destroy(&pool->mutex);
        (void)pthread_cond_destroy(&pool->cond_not_empty);
        (void)pthread_cond_destroy(&pool->cond_not_full);
        free(pool->threads);
        free(pool->tasks);
        free(pool->task_args);
        return -1;
    }

    for (size_t i = 0U; i < thread_count; ++i) {
        if (pthread_create(&pool->threads[i], NULL, sys_threadpool_worker, pool) != 0) {
            pool->shutdown = 1;
            (void)pthread_cond_broadcast(&pool->cond_not_empty);
            for (size_t j = 0U; j < i; ++j) {
                (void)pthread_join(pool->threads[j], NULL);
            }
            (void)pthread_mutex_destroy(&pool->mutex);
            (void)pthread_cond_destroy(&pool->cond_not_empty);
            (void)pthread_cond_destroy(&pool->cond_not_full);
            free(pool->threads);
            free(pool->tasks);
            free(pool->task_args);
            return -1;
        }
    }

    return 0;
}

int sys_threadpool_submit(sys_threadpool *pool, sys_threadpool_task_fn fn, void *arg)
{
    if (pool == NULL || fn == NULL) {
        return -1;
    }

    if (pthread_mutex_lock(&pool->mutex) != 0) {
        return -1;
    }

    while (pool->count == pool->queue_capacity && !pool->shutdown) {
        if (pthread_cond_wait(&pool->cond_not_full, &pool->mutex) != 0) {
            (void)pthread_mutex_unlock(&pool->mutex);
            return -1;
        }
    }

    if (pool->shutdown) {
        (void)pthread_mutex_unlock(&pool->mutex);
        return -1;
    }

    pool->tasks[pool->tail] = fn;
    pool->task_args[pool->tail] = arg;
    pool->tail = (pool->tail + 1U) % pool->queue_capacity;
    pool->count++;

    (void)pthread_cond_signal(&pool->cond_not_empty);
    (void)pthread_mutex_unlock(&pool->mutex);
    return 0;
}

void sys_threadpool_shutdown(sys_threadpool *pool)
{
    if (pool == NULL) {
        return;
    }

    if (pthread_mutex_lock(&pool->mutex) != 0) {
        return;
    }

    pool->shutdown = 1;
    (void)pthread_cond_broadcast(&pool->cond_not_empty);
    (void)pthread_cond_broadcast(&pool->cond_not_full);
    (void)pthread_mutex_unlock(&pool->mutex);

    for (size_t i = 0U; i < pool->thread_count; ++i) {
        (void)pthread_join(pool->threads[i], NULL);
    }

    (void)pthread_mutex_destroy(&pool->mutex);
    (void)pthread_cond_destroy(&pool->cond_not_empty);
    (void)pthread_cond_destroy(&pool->cond_not_full);

    free(pool->threads);
    free(pool->tasks);
    free(pool->task_args);

    pool->threads = NULL;
    pool->tasks = NULL;
    pool->task_args = NULL;
    pool->thread_count = 0U;
    pool->queue_capacity = 0U;
    pool->head = 0U;
    pool->tail = 0U;
    pool->count = 0U;
}

