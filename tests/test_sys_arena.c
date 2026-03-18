#include "sys_toolkit/sys_arena.h"

#include <stdio.h>

int test_sys_arena(void)
{
    sys_arena arena;
    if (sys_arena_init(&arena, 1024U) != 0) {
        (void)printf("sys_arena_init failed\n");
        return 1;
    }

    void *a = sys_arena_alloc(&arena, 128U);
    void *b = sys_arena_alloc(&arena, 256U);
    if (a == NULL || b == NULL) {
        (void)printf("sys_arena_alloc returned NULL\n");
        sys_arena_destroy(&arena);
        return 1;
    }

    sys_arena_reset(&arena);
    void *c = sys_arena_alloc(&arena, 1024U);
    if (c == NULL) {
        (void)printf("sys_arena did not reuse memory after reset\n");
        sys_arena_destroy(&arena);
        return 1;
    }

    sys_arena_destroy(&arena);
    return 0;
}

