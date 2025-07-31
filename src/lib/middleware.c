#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ecewo.h"
#include "middleware.h"

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

    // Find the matching route in the routes array
    const char *path = req->path ? req->path : "";
    const char *method = req->method ? req->method : "";
    MiddlewareInfo *middleware_info = NULL;

    for (size_t i = 0; i < route_count; i++)
    {
        if (routes[i].method && routes[i].path &&
            strcmp(routes[i].method, method) == 0 &&
            matcher(path, routes[i].path))
        {
            middleware_info = (MiddlewareInfo *)routes[i].middleware_ctx;
            break;
        }
    }

    if (!middleware_info)
    {
        printf("Error: No middleware info found for route %s %s\n", method, path);
        return;
    }

    // Calculate total middleware count (global + route-specific)
    int total_middleware_count = global_middleware_count + middleware_info->middleware_count;

    // If no middleware, just call the handler directly
    if (total_middleware_count == 0)
    {
        if (middleware_info->handler)
        {
            middleware_info->handler(req, res);
        }
        return;
    }

    // Allocate memory for the combined middleware handlers
    MiddlewareHandler *combined_handlers = malloc(sizeof(MiddlewareHandler) * total_middleware_count);
    if (!combined_handlers)
    {
        printf("Memory allocation failed for middleware handlers\n");
        // If memory allocation fails, call the route handler directly
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

    // Start the middleware chain execution
    int result = next(&chain, req, res);

    // Free the combined handlers
    free(combined_handlers);

    // Return value meanings:
    // -1: Error (e.g., NULL pointer)
    //  0: Middleware chain stopped (e.g., auth failed) - NORMAL CASE
    //  1: Chain completed successfully and handler was called

    // Only call the handler directly if an actual error occurred (-1)
    if (result == -1)
    {
        printf("ERROR: Middleware chain failed, calling handler directly as fallback\n");
        if (middleware_info->handler)
        {
            middleware_info->handler(req, res);
        }
    }
}

// Helper function to expand the routes array when needed
static void expand_routes(void)
{
    if (route_count >= routes_capacity)
    {
        size_t new_capacity = routes_capacity * 2; // Double the capacity

        // Check for potential integer overflow
        if (new_capacity < routes_capacity)
        {
            fprintf(stderr, "Error: Routes capacity overflow\n");
            exit(EXIT_FAILURE);
        }

        // Calculate the new size with error checking
        size_t new_size = new_capacity * sizeof(Router);
        if (new_size / sizeof(Router) != new_capacity)
        {
            fprintf(stderr, "Error: Routes size calculation overflow\n");
            exit(EXIT_FAILURE);
        }

        Router *new_routes = (Router *)realloc(routes, new_size);
        if (new_routes == NULL)
        {
            fprintf(stderr, "Error: Failed to reallocate memory for routes\n");
            return; // Return instead of exit to allow for error handling
        }

        routes = new_routes;
        routes_capacity = new_capacity;
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

    expand_routes();

    if (!method || !path)
    {
        printf("Error: NULL method or path provided\n");
        return;
    }

    // Create middleware info
    MiddlewareInfo *middleware_info = malloc(sizeof(MiddlewareInfo));
    if (!middleware_info)
    {
        printf("Memory allocation failed for middleware info\n");
        return;
    }

    // Initialize middleware info structure
    middleware_info->middleware = NULL;
    middleware_info->middleware_count = 0;
    middleware_info->handler = handler;

    // Allocate and copy middleware handlers if needed
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

    // Register route with the wrapper handler
    routes[route_count].method = strdup(method); // Make a copy to avoid dangling pointers
    if (!routes[route_count].method)
    {
        printf("Memory allocation failed for route method\n");
        free_middleware_info(middleware_info);
        return;
    }

    routes[route_count].path = strdup(path); // Make a copy to avoid dangling pointers
    if (!routes[route_count].path)
    {
        printf("Memory allocation failed for route path\n");
        free((void *)routes[route_count].method);
        free_middleware_info(middleware_info);
        return;
    }

    routes[route_count].handler = route_handler_with_middleware;
    routes[route_count].middleware_ctx = middleware_info; // Store middleware context

    route_count++;
}

// Cleanup function to free all allocated resources
void reset_middleware()
{
    for (size_t i = 0; i < route_count; i++)
    {
        if (routes[i].middleware_ctx)
        {
            free_middleware_info((MiddlewareInfo *)routes[i].middleware_ctx);
            routes[i].middleware_ctx = NULL;
        }

        // Free duplicated strings
        if (routes[i].method)
        {
            free((void *)routes[i].method);
            routes[i].method = NULL;
        }

        if (routes[i].path)
        {
            free((void *)routes[i].path);
            routes[i].path = NULL;
        }
    }

    // Free global middleware
    if (global_middleware)
    {
        free(global_middleware);
        global_middleware = NULL;
    }
    global_middleware_count = 0;
    global_middleware_capacity = 0;
}
