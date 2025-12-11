#include "ecewo.h"
#include "request.h"

static const char *get_req(const request_t *request, const char *key)
{
    if (!request || !request->items || !key || request->count == 0)
        return NULL;

    size_t key_len = strlen(key);

    for (uint16_t i = 0; i < request->count; i++)
    {
        if (request->items[i].key &&
            strlen(request->items[i].key) == key_len &&
            memcmp(request->items[i].key, key, key_len) == 0)
        {
            return request->items[i].value;
        }
    }

    return NULL;
}

const char *get_param(const Req *req, const char *key)
{
    return req ? get_req(&req->params, key) : NULL;
}

const char *get_query(const Req *req, const char *key)
{
    return req ? get_req(&req->query, key) : NULL;
}

const char *get_header(const Req *req, const char *key)
{
    return req ? get_req(&req->headers, key) : NULL;
}

void set_context(Req *req, const char *key, void *data, size_t size)
{
    if (!req || !req->ctx || !key || !data)
        return;

    context_t *ctx = req->ctx;

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        if (ctx->entries[i].key && strcmp(ctx->entries[i].key, key) == 0)
        {
            ctx->entries[i].data = arena_alloc(req->arena, size);
            if (!ctx->entries[i].data)
                return;
            arena_memcpy(ctx->entries[i].data, data, size);
            ctx->entries[i].size = size;
            return;
        }
    }

    if (ctx->count >= ctx->capacity)
    {
        uint32_t new_capacity = ctx->capacity == 0 ? 8 : ctx->capacity * 2;

        context_entry_t *new_entries = arena_realloc(req->arena,
                                                     ctx->entries,
                                                     ctx->capacity * sizeof(context_entry_t),
                                                     new_capacity * sizeof(context_entry_t));

        if (!new_entries)
            return;

        for (uint32_t i = ctx->capacity; i < new_capacity; i++)
        {
            new_entries[i].key = NULL;
            new_entries[i].data = NULL;
            new_entries[i].size = 0;
        }

        ctx->entries = new_entries;
        ctx->capacity = new_capacity;
    }

    context_entry_t *entry = &ctx->entries[ctx->count];

    entry->key = arena_strdup(req->arena, key);
    if (!entry->key)
        return;

    entry->data = arena_alloc(req->arena, size);
    if (!entry->data)
        return;

    arena_memcpy(entry->data, data, size);
    entry->size = size;

    ctx->count++;
}

void *get_context(Req *req, const char *key)
{
    if (!req || !key)
        return NULL;

    context_t *ctx = req->ctx;

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        if (ctx->entries[i].key && strcmp(ctx->entries[i].key, key) == 0)
            return ctx->entries[i].data;
    }

    return NULL;
}
