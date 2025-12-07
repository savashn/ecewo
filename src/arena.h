#ifndef ECEWO_ARENA_H
#define ECEWO_ARENA_H

#include "ecewo.h"

struct ArenaRegion
{
    struct ArenaRegion *next;
    size_t count;
    size_t capacity;
    uintptr_t data[];
};

void arena_reset(Arena *a);
ArenaRegion *new_region(size_t capacity);

// Pool
void arena_pool_init(void);
void arena_pool_destroy(void);
Arena *arena_pool_acquire(void);
void arena_pool_release(Arena *arena);


#endif
