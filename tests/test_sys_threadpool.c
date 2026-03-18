#include "sys_toolkit/sys_threadpool.h"

#include <stdio.h>

typedef struct counter_ctx {
    int value;
    pthread_mutex_t mutex;
} counter_ctx;

static void increment_task(void *arg)
{
    counter_ctx *ctx = (counter_ctx *)arg;
    if (pthread_mutex_lock(&ctx->mutex) == 0) {
        ctx->value += 1;
        (void)pthread_mutex_unlock(&ctx->mutex);
    }
}

int test_sys_threadpool(void)
{
    sys_threadpool pool;
    if (sys_threadpool_init(&pool, 4U, 16U) != 0) {
        (void)printf("sys_threadpool_init failed\n");
        return 1;
    }

    counter_ctx ctx;
    ctx.value = 0;
    if (pthread_mutex_init(&ctx.mutex, NULL) != 0) {
        (void)printf("pthread_mutex_init failed\n");
        sys_threadpool_shutdown(&pool);
        return 1;
    }

    const int tasks = 32;
    for (int i = 0; i < tasks; ++i) {
        if (sys_threadpool_submit(&pool, increment_task, &ctx) != 0) {
            (void)printf("sys_threadpool_submit failed at %d\n", i);
            (void)pthread_mutex_destroy(&ctx.mutex);
            sys_threadpool_shutdown(&pool);
            return 1;
        }
    }

    sys_threadpool_shutdown(&pool);

    (void)pthread_mutex_lock(&ctx.mutex);
    int value = ctx.value;
    (void)pthread_mutex_unlock(&ctx.mutex);
    (void)pthread_mutex_destroy(&ctx.mutex);

    if (value != tasks) {
        (void)printf("threadpool counter mismatch: expected %d, got %d\n", tasks, value);
        return 1;
    }

    return 0;
}

