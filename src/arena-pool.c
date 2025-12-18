#include "arena.h"
#include "uv.h"
#include <stdlib.h>
#include <stdint.h>

#ifndef ARENA_POOL_SIZE
#define ARENA_POOL_SIZE 1024
#endif

#ifndef PREALLOCATED_ARENA
#define PREALLOCATED_ARENA 32
#endif

#ifndef ARENA_POOL_LOW_WATERMARK
#define ARENA_POOL_LOW_WATERMARK 8  /* Grow when <= 8 available */
#endif

#ifndef ARENA_POOL_HIGH_WATERMARK
#define ARENA_POOL_HIGH_WATERMARK 64  /* Shrink when >= 64 available */
#endif

#ifndef ARENA_POOL_GROW_BATCH
#define ARENA_POOL_GROW_BATCH 8  /* Allocate 8 at a time */
#endif

typedef struct
{
    Arena *arenas[ARENA_POOL_SIZE];
    uint16_t head;
    uint16_t peak_usage;
    uint16_t total_allocated;
    uint16_t grow_count;
    uint16_t shrink_count;
    uv_mutex_t mutex;
    bool initialized;
} arena_pool_t;

static arena_pool_t arena_pool = {0};

// Called when acquiring
static void arena_pool_try_grow(void)
{
    // Already at mutex lock when called
    
    if (arena_pool.head > ARENA_POOL_LOW_WATERMARK)
        return;
    
    uint16_t space_available = ARENA_POOL_SIZE - arena_pool.head;
    if (space_available == 0)
        return;
    
    uint8_t to_allocate = ARENA_POOL_GROW_BATCH;
    if (to_allocate > space_available)
        to_allocate = space_available;
    
    uint8_t allocated = 0;

    for (uint8_t i = 0; i < to_allocate; i++)
    {
        Arena *arena = calloc(1, sizeof(Arena));
        if (!arena)
            break;
        
        arena->end = new_region(ARENA_REGION_SIZE);
        if (!arena->end)
        {
            free(arena);
            break;
        }
        
        arena->begin = arena->end;
        arena_pool.arenas[arena_pool.head++] = arena;
        arena_pool.total_allocated++;
        allocated++;
    }
    
    if (allocated > 0)
    {
        arena_pool.grow_count++;
        LOG_DEBUG("Arena pool grew: +%d arenas (now %d/%d available)",
                  allocated,
                  arena_pool.head,
                  ARENA_POOL_SIZE);
    }
}

// Called when releasing
static void arena_pool_try_shrink(void)
{
    // Already at mutex lock when called
    
    if (arena_pool.head < ARENA_POOL_HIGH_WATERMARK)
        return;
    
    // Keep some reserve, don't shrink below initial size
    uint8_t target = PREALLOCATED_ARENA + ARENA_POOL_GROW_BATCH;
    if (arena_pool.head <= target)
        return;
    
    // Shrink by half of excess
    uint16_t excess = arena_pool.head - target;
    uint16_t to_free = excess / 2;
    if (to_free < ARENA_POOL_GROW_BATCH)
        to_free = ARENA_POOL_GROW_BATCH;
    
    uint16_t freed = 0;
    while (to_free > 0 && arena_pool.head > target)
    {
        Arena *arena = arena_pool.arenas[--arena_pool.head];
        arena_pool.arenas[arena_pool.head] = NULL;
        
        if (arena)
        {
            arena_free(arena);
            free(arena);
            freed++;
        }
        
        to_free--;
    }
    
    if (freed > 0)
    {
        arena_pool.shrink_count++;
        LOG_DEBUG("Arena pool shrunk: -%d arenas (now %d/%d available)",
                  freed, arena_pool.head, ARENA_POOL_SIZE);
    }
}

void arena_pool_init(void)
{
    if (arena_pool.initialized)
        return;

    arena_pool.head = 0;
    arena_pool.peak_usage = 0;
    arena_pool.total_allocated = 0;
    arena_pool.grow_count = 0;
    arena_pool.shrink_count = 0;
    
    if (uv_mutex_init(&arena_pool.mutex) != 0)
    {
        LOG_ERROR("Failed to initialize arena pool mutex");
        abort();
    }

    uint16_t preallocate = PREALLOCATED_ARENA;
    
    const char *env_prealloc = getenv("ECEWO_ARENA_PREALLOC");
    if (env_prealloc)
    {
        uint16_t env_val = atoi(env_prealloc);
        
        if (env_val <= 0)
        {
            LOG_DEBUG("Invalid ECEWO_ARENA_PREALLOC='%s', using default: %d",
                       env_prealloc, preallocate);
        }
        else if (env_val > ARENA_POOL_SIZE)
        {
            LOG_DEBUG("ECEWO_ARENA_PREALLOC=%d exceeds maximum %d, capping to %d",
                       env_val, ARENA_POOL_SIZE, ARENA_POOL_SIZE);
            preallocate = ARENA_POOL_SIZE;
        }
        else
        {
            preallocate = env_val;
            LOG_DEBUG("Using ECEWO_ARENA_PREALLOC=%d from environment", preallocate);
        }
    }

    // Pre-allocate arenas
    uint16_t allocated = 0;
    for (uint16_t i = 0; i < preallocate; i++)
    {
        Arena *arena = calloc(1, sizeof(Arena));
        if (!arena)
        {
            LOG_DEBUG("Failed to allocate arena %d/%d, stopping pre-allocation",
                       i + 1, preallocate);
            break;
        }
        
        // Pre-allocate first region
        arena->end = new_region(ARENA_REGION_SIZE);
        if (!arena->end)
        {
            free(arena);
            LOG_DEBUG("Failed to allocate region for arena %d/%d, stopping",
                       i + 1, preallocate);
            break;
        }
        
        arena->begin = arena->end;
        arena_pool.arenas[arena_pool.head++] = arena;
        allocated++;
        arena_pool.total_allocated++;
    }

    arena_pool.initialized = true;
    
    double allocated_mb = (allocated * ARENA_REGION_SIZE) / (1024.0 * 1024.0);
    LOG_DEBUG("Arena pool initialized: %d/%d arenas (%.2f MB)",
              allocated,
              ARENA_POOL_SIZE,
              allocated_mb);
}

void arena_pool_destroy(void)
{
    if (!arena_pool.initialized)
        return;

    uv_mutex_lock(&arena_pool.mutex);

    // Statistics before destruction
    if (arena_pool.grow_count > 0 || arena_pool.shrink_count > 0)
    {
        LOG_DEBUG("Arena pool statistics:");
        LOG_DEBUG("  Total allocated: %d arenas", arena_pool.total_allocated);
        LOG_DEBUG("  Peak usage: %d arenas", arena_pool.peak_usage);
        LOG_DEBUG("  Grow operations: %d", arena_pool.grow_count);
        LOG_DEBUG("  Shrink operations: %d", arena_pool.shrink_count);
    }

    for (int i = 0; i < arena_pool.head; i++)
    {
        if (arena_pool.arenas[i])
        {
            arena_free(arena_pool.arenas[i]);
            free(arena_pool.arenas[i]);
            arena_pool.arenas[i] = NULL;
        }
    }

    arena_pool.head = 0;
    arena_pool.initialized = false;

    uv_mutex_unlock(&arena_pool.mutex);
    uv_mutex_destroy(&arena_pool.mutex);

    LOG_DEBUG("Arena pool destroyed");
}

Arena *arena_pool_acquire(void)
{
    if (!arena_pool.initialized)
    {
        // Fallback: direct allocation
        LOG_DEBUG("Arena pool not initialized, falling back to direct allocation");
        Arena *arena = calloc(1, sizeof(Arena));
        return arena;
    }

    uv_mutex_lock(&arena_pool.mutex);

    Arena *arena = NULL;

    if (arena_pool.head > 0)
    {
        // Take from pool
        arena = arena_pool.arenas[--arena_pool.head];
        arena_pool.arenas[arena_pool.head] = NULL;
        
        // Update peak usage
        int in_use = arena_pool.total_allocated - arena_pool.head;
        if (in_use > arena_pool.peak_usage)
            arena_pool.peak_usage = in_use;
        
        // Try to grow if running low
        arena_pool_try_grow();
        
        uv_mutex_unlock(&arena_pool.mutex);
        arena_reset(arena);
        return arena;
    }

    // Pool is empty, check if we can grow
    if (arena_pool.total_allocated < ARENA_POOL_SIZE)
    {
        // Allocate new arena
        arena = calloc(1, sizeof(Arena));
        if (arena)
        {
            arena->end = new_region(ARENA_REGION_SIZE);
            if (arena->end)
            {
                arena->begin = arena->end;
                arena_pool.total_allocated++;
                
                int in_use = arena_pool.total_allocated - arena_pool.head;
                if (in_use > arena_pool.peak_usage)
                    arena_pool.peak_usage = in_use;
                
                LOG_DEBUG("Arena pool: allocated new arena (total=%d/%d)",
                          arena_pool.total_allocated, ARENA_POOL_SIZE);
            }
            else
            {
                free(arena);
                arena = NULL;
            }
        }
    }
    else
    {
        LOG_DEBUG("Arena pool exhausted! (max %d reached)", ARENA_POOL_SIZE);
    }

    uv_mutex_unlock(&arena_pool.mutex);
    return arena;
}

void arena_pool_release(Arena *arena)
{
    if (!arena)
        return;

    if (!arena_pool.initialized)
    {
        arena_free(arena);
        free(arena);
        return;
    }

    // Keep only the first region, free the rest
    if (arena->begin && arena->begin->next)
    {
        ArenaRegion *to_free = arena->begin->next;
        arena->begin->next = NULL;
        
        while (to_free)
        {
            ArenaRegion *next = to_free->next;
            free(to_free);
            to_free = next;
        }
    }
    
    // Reset the first region
    if (arena->begin)
    {
        arena->begin->count = 0;
        arena->end = arena->begin;
    }
    
    uv_mutex_lock(&arena_pool.mutex);

    if (arena_pool.head < ARENA_POOL_SIZE)
    {
        // Return to pool
        arena_pool.arenas[arena_pool.head++] = arena;
        
        // Try to shrink if too many available
        arena_pool_try_shrink();
        
        uv_mutex_unlock(&arena_pool.mutex);
    }
    else
    {
        // Pool is full, free
        uv_mutex_unlock(&arena_pool.mutex);
        arena_free(arena);
        free(arena);
    }
}

void arena_pool_print_stats(void)
{
    if (!arena_pool.initialized)
    {
        LOG_DEBUG("Arena pool not initialized");
        return;
    }
    
    uv_mutex_lock(&arena_pool.mutex);
    
    uint16_t available = arena_pool.head;
    uint16_t in_use = arena_pool.total_allocated - available;
    double available_mb = (available * ARENA_REGION_SIZE) / (1024.0 * 1024.0);
    double total_mb = (arena_pool.total_allocated * ARENA_REGION_SIZE) / (1024.0 * 1024.0);
    
    LOG_DEBUG("Arena Pool Statistics:");
    LOG_DEBUG("  Available: %d/%d arenas (%.2f MB)", 
             available, arena_pool.total_allocated, available_mb);
    LOG_DEBUG("  In use: %d arenas", in_use);
    LOG_DEBUG("  Peak usage: %d arenas", arena_pool.peak_usage);
    LOG_DEBUG("  Total allocated: %.2f MB", total_mb);
    LOG_DEBUG("  Grow operations: %d", arena_pool.grow_count);
    LOG_DEBUG("  Shrink operations: %d", arena_pool.shrink_count);
    
    uv_mutex_unlock(&arena_pool.mutex);
}

bool arena_pool_is_initialized(void)
{
    return arena_pool.initialized;
}
