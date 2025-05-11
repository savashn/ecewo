#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include "router.h"

// Function pointer type for middleware
typedef int (*MiddlewareHandler)(Req *req, Res *res, void *next_ctx);

typedef struct
{
    MiddlewareHandler *handlers;
    int count;
    int is_middleware_array;
} MiddlewareArray;

// Maximum number of middleware functions that can be registered
// #define MAX_GLOBAL_MIDDLEWARE 20
// #define MAX_ROUTE_MIDDLEWARE 10

// Structure to store middleware chain context
typedef struct
{
    MiddlewareHandler *handlers;  // Array of middleware handlers
    int count;                    // Number of handlers in the chain
    int current;                  // Current position in the middleware chain
    RequestHandler route_handler; // The final route handler
} Chain;

// Global middleware array
// extern MiddlewareHandler global_middleware[MAX_GLOBAL_MIDDLEWARE];
#define INITIAL_MW_CAPACITY 4

extern MiddlewareHandler *global_middleware;
extern int global_middleware_count;

// Function to add global middleware
void hook(MiddlewareHandler middleware_handler);

// Function to execute the next middleware or route handler in the chain
int next(Chain *chain, Req *req, Res *res);

// Special value used to denote end of variadic arguments
#define MIDDLEWARE_END NULL

// Implementation helper functions
void register_route_with_middleware(const char *method, const char *path,
                                    MiddlewareHandler *middleware, int middleware_count,
                                    RequestHandler handler);

void register_route(const char *method, const char *path, MiddlewareArray middleware, RequestHandler handler);
void free_mw();

// Middleware array creator macro
#define use(...) (MiddlewareArray){(MiddlewareHandler[]){__VA_ARGS__}, sizeof((MiddlewareHandler[]){__VA_ARGS__}) / sizeof(MiddlewareHandler), 1}

// Default empty middleware array
#define NO_MW (MiddlewareArray){NULL, 0, 1}

#endif
