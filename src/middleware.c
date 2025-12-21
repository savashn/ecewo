#include <stdlib.h>
#include "middleware.h"
#include "route-trie.h"
#include "server.h"
#include "logger.h"

MiddlewareHandler *global_middleware = NULL;
uint16_t global_middleware_count = 0;
uint16_t global_middleware_capacity = 0;

int next(Req *req, Res *res, Chain *chain)
{
    if (!chain || !req || !res)
    {
        LOG_ERROR("NULL middleware chain, request or response.");
        return -1;
    }

    if (chain->current < chain->count)
    {
        MiddlewareHandler next_middleware = chain->handlers[chain->current++];
        if (next_middleware)
        {
            return next_middleware(req, res, chain);
        }
        else
        {
            LOG_DEBUG("NULL middleware handler at position %d", chain->current - 1);
            return next(req, res, chain);
        }
    }
    else
    {
        if (chain->route_handler)
        {
            chain->route_handler(req, res);
            return 1;
        }
        return 0;
    }
}

static int execute(Req *req, Res *res, MiddlewareInfo *middleware_info)
{
    if (!req || !res || !middleware_info || !middleware_info->handler)
        return -1;

    int total_middleware_count = global_middleware_count + middleware_info->middleware_count;

    if (total_middleware_count == 0)
    {
        middleware_info->handler(req, res);
        return 0;
    }

    MiddlewareHandler *combined_handlers = arena_alloc(
        req->arena,
        sizeof(MiddlewareHandler) * total_middleware_count);

    if (!combined_handlers)
    {
        LOG_ERROR("Arena allocation failed for middleware handlers.");
        middleware_info->handler(req, res);
        return -1;
    }

    arena_memcpy(combined_handlers, global_middleware,
                 sizeof(MiddlewareHandler) * global_middleware_count);

    if (middleware_info->middleware_count > 0 && middleware_info->middleware)
    {
        arena_memcpy(combined_handlers + global_middleware_count,
                     middleware_info->middleware,
                     sizeof(MiddlewareHandler) * middleware_info->middleware_count);
    }

    Chain *chain = arena_alloc(req->arena, sizeof(Chain));
    if (!chain)
    {
        LOG_ERROR("Arena allocation failed for middleware chain.");
        middleware_info->handler(req, res);
        return -1;
    }

    chain->handlers = combined_handlers;
    chain->count = total_middleware_count;
    chain->current = 0;
    chain->route_handler = middleware_info->handler;

    int result = next(req, res, chain);

    if (result == -1)
    {
        LOG_ERROR("Middleware chain failed.");
        return -1;
    }

    return 0;
}

int execute_handler_with_middleware(
    Req *req,
    Res *res,
    MiddlewareInfo *middleware_info)
{
    if (!req || !res || !middleware_info)
    {
        LOG_ERROR("NULL request, response or middleware info");
        return -1;
    }

    return execute(req, res, middleware_info);
}

void use(MiddlewareHandler middleware_handler)
{
    if (global_middleware_count >= global_middleware_capacity)
    {
        int new_cap = global_middleware_capacity ? global_middleware_capacity * 2 : INITIAL_MW_CAPACITY;
        MiddlewareHandler *tmp = realloc(global_middleware, new_cap * sizeof *tmp);
        if (!tmp)
        {
            LOG_DEBUG("Reallocation failed in global middleware");
            return;
        }
        global_middleware = tmp;
        global_middleware_capacity = new_cap;
    }

    global_middleware[global_middleware_count++] = middleware_handler;
}

void reset_middleware(void)
{
    if (global_middleware)
    {
        free(global_middleware);
        global_middleware = NULL;
    }
    global_middleware_count = 0;
    global_middleware_capacity = 0;
}

void free_middleware_info(MiddlewareInfo *info)
{
    if (info)
    {
        if (info->middleware)
        {
            free(info->middleware);
            info->middleware = NULL;
        }
        free(info);
    }
}
