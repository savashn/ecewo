#ifndef ECEWO_MIDDLEWARE_H
#define ECEWO_MIDDLEWARE_H

#include "ecewo.h"
#include "router.h"
#include "llhttp.h"
#include <stdint.h>

// Chain structure
struct Chain
{
    MiddlewareHandler *handlers;  // Array of middleware handlers
    uint16_t count;               // Number of handlers in the chain
    uint16_t current;             // Current position in the middleware chain
    RequestHandler route_handler; // The final route handler
    handler_type_t handler_type;  // Sync or async execution
};

typedef struct MiddlewareInfo
{
    MiddlewareHandler *middleware;
    uint16_t middleware_count;
    RequestHandler handler;
    handler_type_t handler_type;
} MiddlewareInfo;

#define INITIAL_MW_CAPACITY 4

// Global middleware array
extern MiddlewareHandler *global_middleware;
extern uint16_t global_middleware_count;

// Internal functions
void register_route(llhttp_method_t method,
                    const char *path,
                    MiddlewareArray middleware,
                    RequestHandler handler,
                    handler_type_t type);

void reset_middleware(void);
void free_middleware_info(MiddlewareInfo *info);
void execute_middleware_chain(Req *req, Res *res, MiddlewareInfo *middleware_info);

#endif
