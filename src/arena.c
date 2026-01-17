#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "arena.h"

static inline ArenaRegion *new_region(size_t capacity) {
  size_t size_bytes = sizeof(ArenaRegion) + sizeof(uintptr_t) * capacity;
  ArenaRegion *r = (ArenaRegion *)malloc(size_bytes);

  if (!r)
    return NULL;

  r->next = NULL;
  r->count = 0;
  r->capacity = capacity;
  return r;
}

static inline void free_region(ArenaRegion *r) {
  free(r);
}

bool new_region_to(ArenaRegion **begin, ArenaRegion **end, size_t capacity) {
  ArenaRegion *region = new_region(capacity);
  if (!region) {
    *end = NULL;
    return false;
  }

  *end = region;
  *begin = region;

  return true;
}

void *arena_alloc(Arena *a, size_t size_bytes) {
  size_t size = (size_bytes + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);

  if (a->end == NULL) {
    size_t capacity = ARENA_REGION_SIZE;

    if (capacity < size)
      capacity = size;

    if (!new_region_to(&a->begin, &a->end, capacity))
      return NULL;
  }

  while (a->end->count + size > a->end->capacity && a->end->next != NULL) {
    a->end = a->end->next;
  }

  if (a->end->count + size > a->end->capacity) {
    size_t capacity = ARENA_REGION_SIZE;
    if (capacity < size)
      capacity = size;

    if (!new_region_to(&a->end, &a->end->next, capacity))
      return NULL;
  }

  void *result = &a->end->data[a->end->count];
  a->end->count += size;
  return result;
}

void *arena_realloc(Arena *a, void *oldptr, size_t oldsz, size_t newsz) {
  if (newsz <= oldsz)
    return oldptr;

  void *newptr = arena_alloc(a, newsz);

  if (!newptr)
    return NULL;

  char *newptr_char = (char *)newptr;
  char *oldptr_char = (char *)oldptr;
  for (size_t i = 0; i < oldsz; ++i) {
    newptr_char[i] = oldptr_char[i];
  }
  return newptr;
}

static size_t arena_strlen(const char *s) {
  size_t n = 0;
  while (*s++)
    n++;
  return n;
}

void *arena_memcpy(void *dest, const void *src, size_t n) {
  char *d = dest;
  const char *s = src;

  for (; n; n--)
    *d++ = *s++;

  return dest;
}

char *arena_strdup(Arena *a, const char *cstr) {
  if (!cstr)
    return NULL;

  size_t n = arena_strlen(cstr);
  char *dup = (char *)arena_alloc(a, n + 1);

  if (!dup)
    return NULL;

  arena_memcpy(dup, cstr, n);
  dup[n] = '\0';
  return dup;
}

void *arena_memdup(Arena *a, void *data, size_t size) {
  if (!data || size == 0)
    return NULL;

  void *ptr = arena_alloc(a, size);
  if (!ptr)
    return NULL;

  return arena_memcpy(ptr, data, size);
}

static char *arena_vsprintf(Arena *a, const char *format, va_list args) {
  va_list args_copy;
  va_copy(args_copy, args);
  int n = vsnprintf(NULL, 0, format, args_copy);
  va_end(args_copy);

  if (n < 0)
    return NULL;

  char *result = (char *)arena_alloc(a, n + 1);

  if (!result)
    return NULL;

  vsnprintf(result, n + 1, format, args);

  return result;
}

char *arena_sprintf(Arena *a, const char *format, ...) {
  va_list args;
  va_start(args, format);
  char *result = arena_vsprintf(a, format, args);
  va_end(args);

  return result;
}

void arena_free(Arena *a) {
  ArenaRegion *r = a->begin;
  while (r) {
    ArenaRegion *r0 = r;
    r = r->next;
    free_region(r0);
  }
  a->begin = NULL;
  a->end = NULL;
}

void arena_reset(Arena *a) {
  if (!a || !a->begin)
    return;

  ArenaRegion *region = a->begin;
  while (region) {
    region->count = 0;
    region = region->next;
  }

  a->end = a->begin;
}
