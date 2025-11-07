#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "middleware.h"
#include "route-trie.h"
#include "server.h"
#include "log.h"

MiddlewareHandler *global_middleware = NULL;
uint16_t global_middleware_count = 0;
uint16_t global_middleware_capacity = 0;

typedef struct
{
    uv_work_t work_req;
    RequestHandler handler;
    Req *req;
    Res *res;
    MiddlewareHandler *middleware_handlers;
    uint16_t middleware_count;
    bool completed;
    const char *error_message;
} async_execution_context_t;

// ============================================================================
// ASYNC CALLBACKS
// ============================================================================

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

static int execute_sync(Req *req, Res *res, MiddlewareInfo *middleware_info)
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
    chain->handler_type = middleware_info->handler_type;

    int result = next(req, res, chain);

    if (result == -1)
    {
        LOG_ERROR("Middleware chain failed.");
        return -1;
    }

    return 0;
}

// ============================================================================
// ASYNC EXECUTION
// ============================================================================

static void async_execution_after_work(uv_work_t *req, int status)
{
    async_execution_context_t *ctx = (async_execution_context_t *)req->data;

    decrement_async_work();

    if (!ctx || !server_is_running())
        return;

    if (!ctx->req || !ctx->res || !ctx->req->arena || !ctx->req->client_socket)
    {
        LOG_ERROR("Async execution: Invalid context after work.");
        return;
    }

    if (uv_is_closing((uv_handle_t *)ctx->req->client_socket))
        return;

    if (status < 0)
    {
        LOG_ERROR("Async execution work failed: %s", uv_strerror(status));

        if (server_is_running() && uv_is_writable((uv_stream_t *)ctx->req->client_socket))
        {
            const char *error_msg = "Internal Server Error";
            reply(ctx->res, 500, "text/plain", error_msg, strlen(error_msg));
        }

        return;
    }

    if (!ctx->completed || ctx->error_message)
    {
        LOG_ERROR("Handler execution failed: %s", ctx->error_message ? ctx->error_message : "Unknown error");

        if (server_is_running() && uv_is_writable((uv_stream_t *)ctx->req->client_socket))
        {
            const char *error_msg = "Internal Server Error";
            reply(ctx->res, 500, "text/plain", error_msg, strlen(error_msg));
        }

        return;
    }

    // Success: response should have been sent by handler
    // Arena cleanup is handled by write_completion_cb
}

static void async_execution_work(uv_work_t *req)
{
    async_execution_context_t *ctx = (async_execution_context_t *)req->data;

    if (!ctx || !ctx->handler || !ctx->req || !ctx->res)
    {
        if (ctx)
        {
            ctx->error_message = "Invalid async execution context";
            ctx->completed = false;
        }
        return;
    }

    if (!server_is_running())
    {
        ctx->error_message = "Server is shutting down";
        ctx->completed = false;
        return;
    }

    Chain chain = {
        .handlers = ctx->middleware_handlers,
        .count = ctx->middleware_count,
        .current = 0,
        .route_handler = ctx->handler,
        .handler_type = HANDLER_ASYNC};

    int result = next(ctx->req, ctx->res, &chain);

    if (result == -1)
    {
        ctx->completed = true;
        return;
    }

    ctx->completed = true;
    ctx->error_message = NULL;
}

static int execute_async(Req *req, Res *res, MiddlewareInfo *middleware_info)
{
    if (!req || !res || !middleware_info || !middleware_info->handler)
        return -1;

    if (!server_is_running())
    {
        const char *error_msg = "Service Unavailable";
        reply(res, 503, "text/plain", error_msg, strlen(error_msg));
        return -1;
    }

    // Combine global and route-specific middleware
    int total_mw_count = global_middleware_count + middleware_info->middleware_count;
    MiddlewareHandler *combined_mw = NULL;

    if (total_mw_count > 0)
    {
        combined_mw = arena_alloc(req->arena, sizeof(MiddlewareHandler) * total_mw_count);
        if (!combined_mw)
        {
            const char *error_msg = "Internal Server Error";
            reply(res, 500, "text/plain", error_msg, strlen(error_msg));
            return -1;
        }

        arena_memcpy(combined_mw, global_middleware,
                     sizeof(MiddlewareHandler) * global_middleware_count);

        if (middleware_info->middleware_count > 0 && middleware_info->middleware)
        {
            arena_memcpy(combined_mw + global_middleware_count,
                         middleware_info->middleware,
                         sizeof(MiddlewareHandler) * middleware_info->middleware_count);
        }
    }

    async_execution_context_t *ctx = arena_alloc(req->arena,
                                                 sizeof(async_execution_context_t));
    if (!ctx)
    {
        const char *error_msg = "Internal Server Error";
        reply(res, 500, "text/plain", error_msg, strlen(error_msg));
        return -1;
    }

    ctx->work_req.data = ctx;
    ctx->handler = middleware_info->handler;
    ctx->req = req;
    ctx->res = res;
    ctx->middleware_handlers = combined_mw;
    ctx->middleware_count = total_mw_count;
    ctx->completed = false;
    ctx->error_message = NULL;

    increment_async_work();

    // Queue work in thread pool
    int result = uv_queue_work(
        uv_default_loop(),
        &ctx->work_req,
        async_execution_work,
        async_execution_after_work);

    if (result != 0)
    {
        decrement_async_work();
        return -1;
    }

    return 0;
}

// ============================================================================
// MAIN EXECUTION FUNCTION
// ============================================================================

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

    if (middleware_info->handler_type == HANDLER_ASYNC)
    {
        return execute_async(req, res, middleware_info);
    }
    else
    {
        return execute_sync(req, res, middleware_info);
    }
}

// ============================================================================
// GLOBAL MIDDLEWARE
// ============================================================================

void hook(MiddlewareHandler middleware_handler)
{
    if (global_middleware_count >= global_middleware_capacity)
    {
        int new_cap = global_middleware_capacity ? global_middleware_capacity * 2 : INITIAL_MW_CAPACITY;
        MiddlewareHandler *tmp = realloc(global_middleware, new_cap * sizeof *tmp);
        if (!tmp)
        {
            LOG_DEBUG("Reallocation failed in hook");
            return;
        }
        global_middleware = tmp;
        global_middleware_capacity = new_cap;
    }

    global_middleware[global_middleware_count++] = middleware_handler;
}

// ============================================================================
// ROUTE REGISTRATION
// ============================================================================

static void register_route(llhttp_method_t method,
                           const char *path,
                           MiddlewareArray middleware,
                           RequestHandler handler,
                           handler_type_t type)
{
    if (!handler || !path || !global_route_trie)
    {
        LOG_ERROR("Invalid route registration parameters");
        return;
    }

    MiddlewareInfo *middleware_info = calloc(1, sizeof(MiddlewareInfo));
    if (!middleware_info)
    {
        LOG_DEBUG("Memory allocation failed for middleware info");
        return;
    }

    middleware_info->handler = handler;
    middleware_info->handler_type = type;

    if (middleware.count > 0 && middleware.handlers)
    {
        middleware_info->middleware = malloc(sizeof(MiddlewareHandler) * middleware.count);
        if (!middleware_info->middleware)
        {
            LOG_DEBUG("Memory allocation failed for middleware handlers");
            free(middleware_info);
            return;
        }
        memcpy(middleware_info->middleware, middleware.handlers,
               sizeof(MiddlewareHandler) * middleware.count);
        middleware_info->middleware_count = middleware.count;
    }

    int result = route_trie_add(global_route_trie, method, path, handler, middleware_info);
    if (result != 0)
    {
        LOG_DEBUG("Failed to add route to trie: %d %s", method, path);
        free_middleware_info(middleware_info);
        return;
    }
}

void register_sync_route(int method, const char *path, MiddlewareArray middleware, RequestHandler handler)
{
    register_route((llhttp_method_t)method, path, middleware, handler, HANDLER_SYNC);
}

void register_async_route(int method, const char *path, MiddlewareArray middleware, RequestHandler handler)
{
    register_route((llhttp_method_t)method, path, middleware, handler, HANDLER_ASYNC);
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
