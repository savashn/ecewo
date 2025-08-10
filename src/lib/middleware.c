#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ecewo.h"
#include "middleware.h"
#include "route_trie.h"

// Global middleware
MiddlewareHandler *global_middleware = NULL;
int global_middleware_count = 0;
int global_middleware_capacity = 0;

// Middleware info structure
typedef struct
{
    MiddlewareHandler *middleware;
    int middleware_count;
    RequestHandler handler;
} MiddlewareInfo;

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
int next(Chain *chain, Req *req, Res *res)
{
    if (!chain)
    {
        printf("Error: NULL middleware chain\n");
        return -1;
    }

    if (!req || !res)
    {
        printf("Error: NULL request or response\n");
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
            printf("Warning: NULL middleware handler at position %d\n", chain->current - 1);
            // Skip this middleware and try the next one
            return next(chain, req, res);
        }
    }
    else
    {
        // All middleware executed, call the route handler
        if (chain->route_handler)
        {
            chain->route_handler(req, res);
            return 1; // Successfully executed the route handler
        }
        return 0; // No route handler
    }
}

// Clean up middleware info resources
static void free_middleware_info(MiddlewareInfo *info)
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

// Route handler wrapper function that executes the middleware chain
static void route_handler_with_middleware(Req *req, Res *res)
{
    if (!req || !res)
    {
        printf("Error: NULL request or response\n");
        return;
    }

    route_match_t match;
    if (!global_route_trie || !req->method || !req->path)
    {
        printf("Error: Missing route trie or request info\n");
        return;
    }

    if (!route_trie_match(global_route_trie, req->method, req->path, &match))
    {
        printf("Error: Route not found in trie for %s %s\n", req->method, req->path);
        return;
    }

    MiddlewareInfo *middleware_info = (MiddlewareInfo *)match.middleware_ctx;
    if (!middleware_info)
    {
        printf("Error: No middleware info found for route %s %s\n", req->method, req->path);
        return;
    }

    // Calculate total middleware count
    int total_middleware_count = global_middleware_count + middleware_info->middleware_count;

    // If no middleware, call handler directly
    if (total_middleware_count == 0)
    {
        if (middleware_info->handler)
        {
            middleware_info->handler(req, res);
        }
        return;
    }

    // Allocate memory for combined middleware handlers
    MiddlewareHandler *combined_handlers = malloc(sizeof(MiddlewareHandler) * total_middleware_count);
    if (!combined_handlers)
    {
        printf("Memory allocation failed for middleware handlers\n");
        if (middleware_info->handler)
        {
            middleware_info->handler(req, res);
        }
        return;
    }

    // Copy global middleware handlers first
    memcpy(combined_handlers, global_middleware, sizeof(MiddlewareHandler) * global_middleware_count);

    // Copy route-specific middleware handlers
    if (middleware_info->middleware_count > 0 && middleware_info->middleware)
    {
        memcpy(combined_handlers + global_middleware_count, middleware_info->middleware,
               sizeof(MiddlewareHandler) * middleware_info->middleware_count);
    }

    // Create middleware chain context
    Chain chain = {
        .handlers = combined_handlers,
        .count = total_middleware_count,
        .current = 0,
        .route_handler = middleware_info->handler};

    // Start middleware chain execution
    int result = next(&chain, req, res);

    free(combined_handlers);

    // Error handling
    if (result == -1)
    {
        printf("ERROR: Middleware chain failed, calling handler directly as fallback\n");
        if (middleware_info->handler)
        {
            middleware_info->handler(req, res);
        }
    }
}

// Helper function to register route with middleware
void register_route(const char *method, const char *path, MiddlewareArray middleware, RequestHandler handler)
{
    if (!handler)
    {
        printf("Error: No handler provided for route: %s %s\n", method, path);
        return;
    }

    if (!method || !path)
    {
        printf("Error: NULL method or path provided\n");
        return;
    }

    if (!global_route_trie)
    {
        printf("Error: Route trie not initialized\n");
        return;
    }

    MiddlewareInfo *middleware_info = malloc(sizeof(MiddlewareInfo));
    if (!middleware_info)
    {
        printf("Memory allocation failed for middleware info\n");
        return;
    }

    middleware_info->middleware = NULL;
    middleware_info->middleware_count = 0;
    middleware_info->handler = handler;

    if (middleware.count > 0 && middleware.handlers)
    {
        middleware_info->middleware = malloc(sizeof(MiddlewareHandler) * middleware.count);
        if (!middleware_info->middleware)
        {
            printf("Memory allocation failed for middleware handlers\n");
            free(middleware_info);
            return;
        }
        memcpy(middleware_info->middleware, middleware.handlers, sizeof(MiddlewareHandler) * middleware.count);
        middleware_info->middleware_count = middleware.count;
    }

    int result = route_trie_add(global_route_trie, method, path,
                                route_handler_with_middleware, middleware_info);
    if (result != 0)
    {
        printf("Failed to add route to trie: %s %s\n", method, path);
        free_middleware_info(middleware_info);
        return;
    }
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
