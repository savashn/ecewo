#ifndef ECEWO_MIDDLEWARE_H
#define ECEWO_MIDDLEWARE_H

#include "ecewo.h"
#include "router.h"
#include "llhttp.h"

struct Chain
{
    MiddlewareHandler *handlers;  // Array of middleware handlers
    uint16_t count;               // Number of handlers in the chain
    uint16_t current;             // Current position in the middleware chain
    RequestHandler route_handler; // The final route handler
};

typedef struct MiddlewareInfo
{
    MiddlewareHandler *middleware;
    uint16_t middleware_count;
    RequestHandler handler;
} MiddlewareInfo;

#define INITIAL_MW_CAPACITY 4

extern MiddlewareHandler *global_middleware;
extern uint16_t global_middleware_count;

int execute_handler_with_middleware(Req *req, Res *res, MiddlewareInfo *middleware_info);

void reset_middleware(void);
void free_middleware_info(MiddlewareInfo *info);

#endif
