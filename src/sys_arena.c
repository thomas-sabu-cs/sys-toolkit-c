#include "sys_toolkit/sys_arena.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int sys_arena_init(sys_arena *arena, size_t capacity)
{
    if (arena == NULL || capacity == 0U) {
        return -1;
    }

    arena->base = (unsigned char *)malloc(capacity);
    if (arena->base == NULL) {
        arena->capacity = 0U;
        arena->offset = 0U;
        return -1;
    }

    arena->capacity = capacity;
    arena->offset = 0U;
    return 0;
}

static size_t sys_arena_align(size_t offset)
{
    const size_t alignment = sizeof(max_align_t);
    const size_t mask = alignment - 1U;
    return (offset + mask) & ~mask;
}

void *sys_arena_alloc(sys_arena *arena, size_t size)
{
    if (arena == NULL || arena->base == NULL || size == 0U) {
        return NULL;
    }

    size_t aligned_offset = sys_arena_align(arena->offset);
    if (aligned_offset < arena->offset) {
        return NULL;
    }

    if (size > arena->capacity - aligned_offset) {
        return NULL;
    }

    void *ptr = arena->base + aligned_offset;
    arena->offset = aligned_offset + size;

    assert(arena->offset <= arena->capacity);
    return ptr;
}

void sys_arena_reset(sys_arena *arena)
{
    if (arena == NULL) {
        return;
    }

    arena->offset = 0U;
}

void sys_arena_destroy(sys_arena *arena)
{
    if (arena == NULL) {
        return;
    }

    free(arena->base);
    arena->base = NULL;
    arena->capacity = 0U;
    arena->offset = 0U;
}

