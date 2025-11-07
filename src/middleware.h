#ifndef ECEWO_MIDDLEWARE_H
#define ECEWO_MIDDLEWARE_H

#include "ecewo.h"
#include "router.h"
#include "llhttp.h"
#include <stdint.h>

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

extern MiddlewareHandler *global_middleware;
extern uint16_t global_middleware_count;

int execute_handler_with_middleware(Req *req, Res *res, MiddlewareInfo *middleware_info);

void register_sync_route(int method, const char *path, MiddlewareArray middleware, RequestHandler handler);
void register_async_route(int method, const char *path, MiddlewareArray middleware, RequestHandler handler);

void reset_middleware(void);
void free_middleware_info(MiddlewareInfo *info);

#endif
