#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "middleware.h"
#include "route_trie.h"
#include "arena.h"

// Global middleware (uses malloc since it's long-lived)
MiddlewareHandler *global_middleware = NULL;
uint16_t global_middleware_count = 0;
uint16_t global_middleware_capacity = 0;

// Add middleware to global chain
void hook(MiddlewareHandler middleware_handler)
{
    if (global_middleware_count >= global_middleware_capacity)
    {
        int new_cap = global_middleware_capacity ? global_middleware_capacity * 2 : INITIAL_MW_CAPACITY;
        MiddlewareHandler *tmp = realloc(global_middleware, new_cap * sizeof *tmp);
        if (!tmp)
        {
            perror("realloc");
            return;
        }
        global_middleware = tmp;
        global_middleware_capacity = new_cap;
    }

    global_middleware[global_middleware_count++] = middleware_handler;
}

// Helper function for middleware chain execution
int next(Req *req, Res *res, Chain *chain)
{
    if (!chain)
    {
        fprintf(stderr, "Error: NULL middleware chain\n");
        return -1;
    }

    if (!req || !res)
    {
        fprintf(stderr, "Error: NULL request or response\n");
        return -1;
    }

    // Check if we have more middleware to execute
    if (chain->current < chain->count)
    {
        // Execute the next middleware in the chain
        MiddlewareHandler next_middleware = chain->handlers[chain->current++];
        if (next_middleware)
        {
            return next_middleware(req, res, chain);
        }
        else
        {
            fprintf(stderr, "Warning: NULL middleware handler at position %d\n", chain->current - 1);
            // Skip this middleware and try the next one
            return next(req, res, chain);
        }
    }
    else
    {
        // All middleware executed, call the route handler
        if (chain->route_handler)
        {
            // Check if handler should run async or sync
            if (chain->handler_type == HANDLER_ASYNC)
            {
                // Execute handler asynchronously
                return execute_async_handler(chain->route_handler, req, res);
            }
            else
            {
                // Execute handler synchronously
                chain->route_handler(req, res);
                return 1; // Successfully executed the route handler
            }
        }
        return 0; // No route handler
    }
}

// Clean up middleware info resources
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

// Function that runs the middleware chain
void execute_middleware_chain(Req *req, Res *res, MiddlewareInfo *middleware_info)
{
    if (!req || !res || !middleware_info)
    {
        fprintf(stderr, "Error: NULL request, response, or middleware info\n");
        return;
    }

    int total_middleware_count = global_middleware_count + middleware_info->middleware_count;

    // If there is no middleware, call the handler directly
    if (total_middleware_count == 0)
    {
        if (middleware_info->handler)
        {
            // Check handler type and execute accordingly
            if (middleware_info->handler_type == HANDLER_ASYNC)
            {
                execute_async_handler(middleware_info->handler, req, res);
            }
            else
            {
                middleware_info->handler(req, res);
            }
        }
        return;
    }

    // Allocate memory for combined middleware handlers
    MiddlewareHandler *combined_handlers = arena_alloc(req->arena,
                                                       sizeof(MiddlewareHandler) * total_middleware_count);
    if (!combined_handlers)
    {
        fprintf(stderr, "Arena allocation failed for middleware handlers\n");
        if (middleware_info->handler)
        {
            // Fallback: execute handler directly
            if (middleware_info->handler_type == HANDLER_ASYNC)
            {
                execute_async_handler(middleware_info->handler, req, res);
            }
            else
            {
                middleware_info->handler(req, res);
            }
        }
        return;
    }

    // Copy global middleware handlers first
    arena_memcpy(combined_handlers, global_middleware, sizeof(MiddlewareHandler) * global_middleware_count);

    // Copy route-specific middleware handlers
    if (middleware_info->middleware_count > 0 && middleware_info->middleware)
    {
        arena_memcpy(combined_handlers + global_middleware_count, middleware_info->middleware,
                     sizeof(MiddlewareHandler) * middleware_info->middleware_count);
    }

    // Create middleware chain context (allocated in request arena)
    Chain *chain = arena_alloc(req->arena, sizeof(Chain));
    if (!chain)
    {
        fprintf(stderr, "Arena allocation failed for middleware chain\n");
        if (middleware_info->handler)
        {
            // Fallback: execute handler directly
            if (middleware_info->handler_type == HANDLER_ASYNC)
            {
                execute_async_handler(middleware_info->handler, req, res);
            }
            else
            {
                middleware_info->handler(req, res);
            }
        }
        return;
    }

    chain->handlers = combined_handlers;
    chain->count = total_middleware_count;
    chain->current = 0;
    chain->route_handler = middleware_info->handler;
    chain->handler_type = middleware_info->handler_type;

    // Start middleware chain execution
    int result = next(req, res, chain);

    // Error handling
    if (result == -1)
    {
        fprintf(stderr, "Error: Middleware chain failed, calling handler directly as fallback\n");
        if (middleware_info->handler)
        {
            if (middleware_info->handler_type == HANDLER_ASYNC)
            {
                execute_async_handler(middleware_info->handler, req, res);
            }
            else
            {
                middleware_info->handler(req, res);
            }
        }
    }
}

// Helper function to register route with middleware (uses malloc for long-lived data)
void register_route(llhttp_method_t method,
                    const char *path,
                    MiddlewareArray middleware,
                    RequestHandler handler,
                    handler_type_t type)
{
    if (!handler)
    {
        fprintf(stderr, "Error: No handler provided for route: %d %s\n", method, path);
        return;
    }

    if (!path)
    {
        fprintf(stderr, "Error: NULL path provided\n");
        return;
    }

    if (!global_route_trie)
    {
        fprintf(stderr, "Error: Route trie not initialized\n");
        return;
    }

    MiddlewareInfo *middleware_info = calloc(1, sizeof(MiddlewareInfo));
    if (!middleware_info)
    {
        fprintf(stderr, "Memory allocation failed for middleware info\n");
        return;
    }

    middleware_info->handler = handler;
    middleware_info->handler_type = type; // Store handler type

    if (middleware.count > 0 && middleware.handlers)
    {
        middleware_info->middleware = malloc(sizeof(MiddlewareHandler) * middleware.count);
        if (!middleware_info->middleware)
        {
            fprintf(stderr, "Memory allocation failed for middleware handlers\n");
            free(middleware_info);
            return;
        }
        memcpy(middleware_info->middleware, middleware.handlers, sizeof(MiddlewareHandler) * middleware.count);
        middleware_info->middleware_count = middleware.count;
    }

    int result = route_trie_add(global_route_trie, method, path, handler, middleware_info);
    if (result != 0)
    {
        fprintf(stderr, "Failed to add route to trie: %d %s\n", method, path);
        free_middleware_info(middleware_info);
        return;
    }
}

// Public API implementations
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
