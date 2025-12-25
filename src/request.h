#ifndef ECEWO_REQUEST_H
#define ECEWO_REQUEST_H

#include <stddef.h>
#include "arena.h"

typedef struct
{
    char *key;
    void *data;
} context_entry_t;

struct context_t
{
    context_entry_t *entries;
    uint32_t count;
    uint32_t capacity;
};

#endif
