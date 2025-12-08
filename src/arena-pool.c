#include "arena.h"
#include "uv.h"
#include <stdlib.h>

#ifndef ARENA_POOL_SIZE
#define ARENA_POOL_SIZE 1024
#endif

#ifndef PREALLOCATED_ARENA
#define PREALLOCATED_ARENA 256
#endif

#define ARENA_MIN_REGION_SIZE (64 * 1024)  // 64KB minimum

typedef struct
{
    Arena *arenas[ARENA_POOL_SIZE];
    int head;
    uv_mutex_t mutex;
    bool initialized;
} arena_pool_t;

static arena_pool_t g_arena_pool = {0};

void arena_pool_init(void)
{
    if (g_arena_pool.initialized)
        return;

    g_arena_pool.head = 0;
    
    if (uv_mutex_init(&g_arena_pool.mutex) != 0)
    {
        LOG_ERROR("Failed to initialize arena pool mutex");
        return;
    }

    // Pre-allocate initial arenas
    for (int i = 0; i < PREALLOCATED_ARENA; i++)
    {
        Arena *arena = calloc(1, sizeof(Arena));
        if (arena)
        {
            // Pre-allocate first region
            arena->end = new_region(ARENA_MIN_REGION_SIZE);
            if (arena->end)
            {
                arena->begin = arena->end;
                g_arena_pool.arenas[g_arena_pool.head++] = arena;
            }
            else
            {
                free(arena);
            }
        }
    }

    g_arena_pool.initialized = true;
    LOG_DEBUG("Arena pool initialized with %d arenas", g_arena_pool.head);
}

void arena_pool_destroy(void)
{
    if (!g_arena_pool.initialized)
        return;

    uv_mutex_lock(&g_arena_pool.mutex);

    for (int i = 0; i < g_arena_pool.head; i++)
    {
        if (g_arena_pool.arenas[i])
        {
            arena_free(g_arena_pool.arenas[i]);
            free(g_arena_pool.arenas[i]);
            g_arena_pool.arenas[i] = NULL;
        }
    }

    g_arena_pool.head = 0;
    g_arena_pool.initialized = false;

    uv_mutex_unlock(&g_arena_pool.mutex);
    uv_mutex_destroy(&g_arena_pool.mutex);

    LOG_DEBUG("Arena pool destroyed");
}

Arena *arena_pool_acquire(void)
{
    if (!g_arena_pool.initialized)
    {
        // Fallback: direct allocation
        Arena *arena = calloc(1, sizeof(Arena));
        return arena;
    }

    uv_mutex_lock(&g_arena_pool.mutex);

    Arena *arena = NULL;

    if (g_arena_pool.head > 0)
    {
        // Take from the pool
        arena = g_arena_pool.arenas[--g_arena_pool.head];
        g_arena_pool.arenas[g_arena_pool.head] = NULL;
    }

    uv_mutex_unlock(&g_arena_pool.mutex);

    if (arena)
    {
        arena_reset(arena);
        return arena;
    }

    // Pool is empt, create a new arena
    arena = calloc(1, sizeof(Arena));
    if (arena)
    {
        arena->end = new_region(ARENA_MIN_REGION_SIZE);
        if (arena->end)
        {
            arena->begin = arena->end;
        }
        else
        {
            free(arena);
            return NULL;
        }
    }

    return arena;
}

void arena_pool_release(Arena *arena)
{
    if (!arena)
        return;

    if (!g_arena_pool.initialized)
    {
        // Fallback: direct free
        arena_free(arena);
        free(arena);
        return;
    }

    arena_reset(arena);
    uv_mutex_lock(&g_arena_pool.mutex);

    if (g_arena_pool.head < ARENA_POOL_SIZE)
    {
        // Put in the pool back
        g_arena_pool.arenas[g_arena_pool.head++] = arena;
        uv_mutex_unlock(&g_arena_pool.mutex);
    }
    else
    {
        // The pool is full, free
        uv_mutex_unlock(&g_arena_pool.mutex);
        arena_free(arena);
        free(arena);
    }
}
