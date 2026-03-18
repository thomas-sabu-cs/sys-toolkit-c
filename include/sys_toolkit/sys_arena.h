/**
 * @file sys_arena.h
 * @brief Simple linear arena allocator for high-throughput systems code.
 *
 * The arena allocator in this module is intentionally minimal: it trades
 * fine-grained deallocation for extremely cheap allocations and predictable
 * behaviour. All memory is released in bulk via ::sys_arena_reset, which is
 * a common pattern in systems that process work in phases (frames, requests,
 * transactions).
 *
 * Design notes:
 * - The implementation is written in C17 and uses only the C standard library.
 * - Allocation is bump-pointer based and therefore cache-friendly.
 * - No implicit growth: the arena never calls realloc; capacity is explicit.
 * - All operations are O(1) and have well-defined failure modes.
 */

#ifndef SYS_ARENA_H
#define SYS_ARENA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Arena allocator backing store.
 *
 * The `sys_arena` structure owns a contiguous memory region and an offset
 * into that region. Allocations simply advance the offset; no metadata is
 * stored alongside individual allocations which keeps fragmentation and
 * bookkeeping overhead at zero.
 */
typedef struct sys_arena {
    unsigned char *base;  /**< Pointer to the start of the buffer. */
    size_t capacity;      /**< Total capacity of the buffer in bytes. */
    size_t offset;        /**< Current bump pointer offset in bytes. */
} sys_arena;

/**
 * @brief Initialize an arena with the given capacity.
 *
 * This function allocates `capacity` bytes using `malloc`. The arena can then
 * be used to service allocations until the capacity is exhausted, at which
 * point ::sys_arena_alloc will return `NULL`.
 *
 * @param arena Pointer to the arena structure to initialize.
 * @param capacity Number of bytes to allocate for the arena.
 * @return 0 on success, non-zero on allocation failure or invalid arguments.
 */
int sys_arena_init(sys_arena *arena, size_t capacity);

/**
 * @brief Allocate a block of memory from the arena.
 *
 * The returned memory is aligned to `alignof(max_align_t)` where possible.
 * The arena does not support individual frees; memory is considered valid
 * until ::sys_arena_reset is called or the arena is destroyed.
 *
 * @param arena Pointer to an initialized arena.
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated memory or `NULL` if the arena is exhausted
 *         or arguments are invalid.
 */
void *sys_arena_alloc(sys_arena *arena, size_t size);

/**
 * @brief Reset the arena to an empty state without releasing memory.
 *
 * This is a constant-time operation that makes all previously allocated
 * memory invalid. The underlying buffer remains allocated and can be reused
 * for subsequent allocations, which is ideal for tight allocation/reuse loops.
 *
 * @param arena Pointer to an initialized arena.
 */
void sys_arena_reset(sys_arena *arena);

/**
 * @brief Destroy the arena and release its backing memory.
 *
 * After destruction, the arena must not be used until reinitialized.
 *
 * @param arena Pointer to an initialized arena.
 */
void sys_arena_destroy(sys_arena *arena);

#ifdef __cplusplus
}
#endif

#endif /* SYS_ARENA_H */

