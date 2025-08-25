#ifndef ECEWO_MIDDLEWARE_H
#define ECEWO_MIDDLEWARE_H

#include "router.h"

// Forward declaration of Chain structure
typedef struct Chain Chain;

// Function pointer type for middleware
typedef int (*MiddlewareHandler)(Req *req, Res *res, Chain *chain);

// Structure to store middleware chain context
struct Chain
{
    MiddlewareHandler *handlers;  // Array of middleware handlers
    int count;                    // Number of handlers in the chain
    int current;                  // Current position in the middleware chain
    RequestHandler route_handler; // The final route handler
};

typedef struct MiddlewareInfo
{
    MiddlewareHandler *middleware;
    int middleware_count;
    RequestHandler handler;
} MiddlewareInfo;

typedef struct
{
    MiddlewareHandler *handlers;
    size_t count;
} MiddlewareArray;

#define use(...)                                        \
    ((MiddlewareArray){                                 \
        .handlers = (MiddlewareHandler[]){__VA_ARGS__}, \
        .count = sizeof((MiddlewareHandler[]){__VA_ARGS__}) / sizeof(MiddlewareHandler)})

#define NO_MW ((MiddlewareArray){.handlers = NULL, .count = 0})

#define INITIAL_MW_CAPACITY 4

// Global middleware array
extern MiddlewareHandler *global_middleware;
extern int global_middleware_count;

// Function to add global middleware
void hook(MiddlewareHandler middleware_handler);

// Function to execute the next middleware or route handler in the chain
int next(Chain *chain, Req *req, Res *res);

void register_route(const char *method, const char *path,
                    MiddlewareArray middleware,
                    RequestHandler handler);

void reset_middleware(void);

void free_middleware_info(MiddlewareInfo *info);

// The main function that runs middleware chain
void execute_middleware_chain(Req *req, Res *res, MiddlewareInfo *middleware_info);

#endif
