#include "router.h"

Router routes[MAX_ROUTES];
int route_count = 0;

void get(const char *path, ...)
{
    va_list args;
    va_start(args, path);

    // Get the first argument
    void *first_arg = va_arg(args, void *);

    // If the first argument is NULL, throw an error
    if (first_arg == NULL)
    {
        printf("Error: No handler provided for GET route: %s\n", path);
        va_end(args);
        return;
    }

    // Is the first argument a middleware array or a handler?
    // We check the is_middleware_array field of MiddlewareArray
    int is_middleware = 0;

    // Check the last 4 bytes of the first argument to determine if it's a MiddlewareArray
    // We use the offset of the is_middleware_array field
    int *flag_ptr = (int *)((char *)first_arg + offsetof(MiddlewareArray, is_middleware_array));

    // If we can read from this address and the value is 1, it's a middleware array
    if (*flag_ptr == 1)
    {
        is_middleware = 1;
    }

    if (is_middleware)
    {
        // The first argument is a middleware array
        MiddlewareArray *middleware = (MiddlewareArray *)first_arg;

        // The second argument should be the handler
        RequestHandler handler = (RequestHandler)va_arg(args, void *);

        if (handler == NULL)
        {
            printf("Error: No handler provided after middleware for GET route: %s\n", path);
            va_end(args);
            return;
        }

        register_route("GET", path, *middleware, handler);
    }
    else
    {
        // The first argument is a handler, no middleware provided
        register_route("GET", path, NO_MW, (RequestHandler)first_arg);
    }

    va_end(args);
}

void post(const char *path, ...)
{
    va_list args;
    va_start(args, path);

    void *first_arg = va_arg(args, void *);

    if (first_arg == NULL)
    {
        printf("Error: No handler provided for POST route: %s\n", path);
        va_end(args);
        return;
    }

    int is_middleware = 0;

    int *flag_ptr = (int *)((char *)first_arg + offsetof(MiddlewareArray, is_middleware_array));

    if (*flag_ptr == 1)
    {
        is_middleware = 1;
    }

    if (is_middleware)
    {
        MiddlewareArray *middleware = (MiddlewareArray *)first_arg;

        RequestHandler handler = (RequestHandler)va_arg(args, void *);

        if (handler == NULL)
        {
            printf("Error: No handler provided after middleware for POST route: %s\n", path);
            va_end(args);
            return;
        }

        register_route("POST", path, *middleware, handler);
    }
    else
    {
        register_route("POST", path, NO_MW, (RequestHandler)first_arg);
    }

    va_end(args);
}

void put(const char *path, ...)
{
    va_list args;
    va_start(args, path);

    void *first_arg = va_arg(args, void *);

    if (first_arg == NULL)
    {
        printf("Error: No handler provided for PUT route: %s\n", path);
        va_end(args);
        return;
    }

    int is_middleware = 0;

    int *flag_ptr = (int *)((char *)first_arg + offsetof(MiddlewareArray, is_middleware_array));

    if (*flag_ptr == 1)
    {
        is_middleware = 1;
    }

    if (is_middleware)
    {
        MiddlewareArray *middleware = (MiddlewareArray *)first_arg;

        RequestHandler handler = (RequestHandler)va_arg(args, void *);

        if (handler == NULL)
        {
            printf("Error: No handler provided after middleware for PUT route: %s\n", path);
            va_end(args);
            return;
        }

        register_route("PUT", path, *middleware, handler);
    }
    else
    {
        register_route("PUT", path, NO_MW, (RequestHandler)first_arg);
    }

    va_end(args);
}

void del(const char *path, ...)
{
    va_list args;
    va_start(args, path);

    void *first_arg = va_arg(args, void *);

    if (first_arg == NULL)
    {
        printf("Error: No handler provided for DELETE route: %s\n", path);
        va_end(args);
        return;
    }

    int is_middleware = 0;

    int *flag_ptr = (int *)((char *)first_arg + offsetof(MiddlewareArray, is_middleware_array));

    if (*flag_ptr == 1)
    {
        is_middleware = 1;
    }

    if (is_middleware)
    {
        MiddlewareArray *middleware = (MiddlewareArray *)first_arg;

        RequestHandler handler = (RequestHandler)va_arg(args, void *);

        if (handler == NULL)
        {
            printf("Error: No handler provided after middleware for DELETE route: %s\n", path);
            va_end(args);
            return;
        }

        register_route("DELETE", path, *middleware, handler);
    }
    else
    {
        register_route("DELETE", path, NO_MW, (RequestHandler)first_arg);
    }

    va_end(args);
}
