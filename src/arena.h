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

#endif