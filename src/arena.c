#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include "arena.h"

#ifndef ARENA_REGION_DEFAULT_CAPACITY
#define ARENA_REGION_DEFAULT_CAPACITY (8*1024)
#endif

#include <assert.h>
#define ARENA_ASSERT assert

static ArenaRegion *new_region(size_t capacity)
{
    size_t size_bytes = sizeof(ArenaRegion) + sizeof(uintptr_t)*capacity;
    ArenaRegion *r = (ArenaRegion*)malloc(size_bytes);
    ARENA_ASSERT(r);
    r->next = NULL;
    r->count = 0;
    r->capacity = capacity;
    return r;
}

static void free_region(ArenaRegion *r)
{
    free(r);
}

void *arena_alloc(Arena *a, size_t size_bytes)
{
    size_t size = (size_bytes + sizeof(uintptr_t) - 1)/sizeof(uintptr_t);

    if (a->end == NULL)
    {
        ARENA_ASSERT(a->begin == NULL);
        size_t capacity = ARENA_REGION_DEFAULT_CAPACITY;
        if (capacity < size) capacity = size;
        a->end = new_region(capacity);
        a->begin = a->end;
    }

    while (a->end->count + size > a->end->capacity && a->end->next != NULL)
    {
        a->end = a->end->next;
    }

    if (a->end->count + size > a->end->capacity)
    {
        ARENA_ASSERT(a->end->next == NULL);
        size_t capacity = ARENA_REGION_DEFAULT_CAPACITY;
        if (capacity < size) capacity = size;
        a->end->next = new_region(capacity);
        a->end = a->end->next;
    }

    void *result = &a->end->data[a->end->count];
    a->end->count += size;
    return result;
}

void *arena_realloc(Arena *a, void *oldptr, size_t oldsz, size_t newsz)
{
    if (newsz <= oldsz) return oldptr;
    void *newptr = arena_alloc(a, newsz);
    char *newptr_char = (char*)newptr;
    char *oldptr_char = (char*)oldptr;
    for (size_t i = 0; i < oldsz; ++i)
    {
        newptr_char[i] = oldptr_char[i];
    }
    return newptr;
}

static size_t arena_strlen(const char *s)
{
    size_t n = 0;
    while (*s++) n++;
    return n;
}

void *arena_memcpy(void *dest, const void *src, size_t n)
{
    char *d = dest;
    const char *s = src;
    for (; n; n--) *d++ = *s++;
    return dest;
}

char *arena_strdup(Arena *a, const char *cstr)
{
    size_t n = arena_strlen(cstr);
    char *dup = (char*)arena_alloc(a, n + 1);
    arena_memcpy(dup, cstr, n);
    dup[n] = '\0';
    return dup;
}

void *arena_memdup(Arena *a, void *data, size_t size)
{
    return arena_memcpy(arena_alloc(a, size), data, size);
}

static char *arena_vsprintf(Arena *a, const char *format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    int n = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    ARENA_ASSERT(n >= 0);
    char *result = (char*)arena_alloc(a, n + 1);
    vsnprintf(result, n + 1, format, args);

    return result;
}

char *arena_sprintf(Arena *a, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char *result = arena_vsprintf(a, format, args);
    va_end(args);

    return result;
}

void arena_free(Arena *a)
{
    ArenaRegion *r = a->begin;
    while (r)
    {
        ArenaRegion *r0 = r;
        r = r->next;
        free_region(r0);
    }
    a->begin = NULL;
    a->end = NULL;
}

void arena_reset(Arena *a)
{
    for (ArenaRegion *r = a->begin; r != NULL; r = r->next)
    {
        r->count = 0;
    }

    a->end = a->begin;
}
