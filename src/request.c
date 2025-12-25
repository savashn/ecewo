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

void set_context(Req *req, const char *key, void *data)
{
    if (!req || !req->ctx || !key)
        return;

    context_t *ctx = req->ctx;

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        if (ctx->entries[i].key && strcmp(ctx->entries[i].key, key) == 0)
        {
            ctx->entries[i].data = data;
            return;
        }
    }

    if (ctx->count >= ctx->capacity)
    {
        uint32_t new_capacity = ctx->capacity == 0 ? 8 : ctx->capacity * 2;

        context_entry_t *new_entries = arena_realloc(
            req->arena,
            ctx->entries,
            ctx->capacity * sizeof(context_entry_t),
            new_capacity * sizeof(context_entry_t)
        );

        if (!new_entries)
            return;

        memset(&new_entries[ctx->capacity], 0, (new_capacity - ctx->capacity) * sizeof(context_entry_t));

        ctx->entries = new_entries;
        ctx->capacity = new_capacity;
    }

    context_entry_t *entry = &ctx->entries[ctx->count];

    entry->key = arena_strdup(req->arena, key);
    if (!entry->key)
        return;

    entry->data = data;
    ctx->count++;
}

void *get_context(Req *req, const char *key)
{
    if (!req || !req->ctx || !key)
        return NULL;

    context_t *ctx = req->ctx;

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        if (ctx->entries[i].key && strcmp(ctx->entries[i].key, key) == 0)
            return ctx->entries[i].data;
    }

    return NULL;
}
