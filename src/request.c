#include "ecewo.h"

static str_t get_req(const request_t *request, const char *key)
{
    if (!request || !request->items || !key || request->count == 0)
        return (str_t){NULL, 0};

    size_t key_len = strlen(key);

    for (uint16_t i = 0; i < request->count; i++)
    {
        if (request->items[i].key.len == key_len &&
            memcmp(request->items[i].key.data, key, key_len) == 0)
        {
            return request->items[i].value;
        }
    }
    
    return (str_t){NULL, 0};
}

static const char *str_to_cstr(Arena *arena, str_t sv)
{
    if (!sv.data || sv.len == 0)
        return NULL;
    
    char *str = arena_alloc(arena, sv.len + 1);
    if (!str)
        return NULL;
        
    memcpy(str, sv.data, sv.len);
    str[sv.len] = '\0';
    return str;
}

const char *get_method(Req *req)
{
    if (!req) return NULL;
    return str_to_cstr(req->arena, req->method);
}

const char *get_path(Req *req)
{
    if (!req) return NULL;
    return str_to_cstr(req->arena, req->path);
}

const char *get_body(Req *req)
{
    if (!req) return NULL;
    return str_to_cstr(req->arena, req->body);
}

size_t get_body_len(Req *req)
{
    return req ? req->body.len : 0;
}

const char *get_param(Req *req, const char *key)
{
    if (!req) return NULL;
    return str_to_cstr(req->arena, get_req(&req->params, key));
}

const char *get_query(Req *req, const char *key)
{
    if (!req) return NULL;
    return str_to_cstr(req->arena, get_req(&req->query, key));
}

const char *get_header(Req *req, const char *key)
{
    if (!req) return NULL;
    return str_to_cstr(req->arena, get_req(&req->headers, key));
}

void set_context(Req *req, const char *key, void *data, size_t size)
{
    if (!req || !key || !data)
        return;

    context_t *ctx = &req->ctx;

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        if (ctx->entries[i].key && strcmp(ctx->entries[i].key, key) == 0)
        {
            ctx->entries[i].data = arena_alloc(ctx->arena, size);
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

        context_entry_t *new_entries = arena_realloc(ctx->arena,
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

    entry->key = arena_strdup(ctx->arena, key);
    if (!entry->key)
        return;

    entry->data = arena_alloc(ctx->arena, size);
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

    context_t *ctx = &req->ctx;

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        if (ctx->entries[i].key && strcmp(ctx->entries[i].key, key) == 0)
            return ctx->entries[i].data;
    }

    return NULL;
}
