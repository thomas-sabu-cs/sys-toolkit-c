#include "sys_toolkit/sys_arena.h"

#include <stdio.h>
#include <time.h>

static double seconds_since(clock_t start, clock_t end)
{
    return (double)(end - start) / (double)CLOCKS_PER_SEC;
}

int main(void)
{
    const size_t capacity = 1024U * 1024U;
    const size_t allocations = 100000U;

    sys_arena arena;
    if (sys_arena_init(&arena, capacity) != 0) {
        (void)printf("arena_bench: sys_arena_init failed\n");
        return 1;
    }

    clock_t start = clock();
    for (size_t i = 0U; i < allocations; ++i) {
        if (sys_arena_alloc(&arena, 64U) == NULL) {
            sys_arena_reset(&arena);
        }
    }
    clock_t end = clock();

    (void)printf("arena_bench: %zu allocations in %.6f seconds\n",
                 allocations, seconds_since(start, end));

    sys_arena_destroy(&arena);
    return 0;
}

