#ifndef ECEWO_ARENA_H
#define ECEWO_ARENA_H

#include "ecewo.h"

#ifndef ARENA_REGION_SIZE
#define ARENA_REGION_SIZE (64 * 1024)
#endif

struct ArenaRegion {
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
bool arena_pool_is_initialized(void);

#endif
