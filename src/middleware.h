#ifndef ECEWO_MIDDLEWARE_H
#define ECEWO_MIDDLEWARE_H

#include "ecewo.h"
#include "llhttp.h"

#ifndef INITIAL_MW_CAPACITY
#define INITIAL_MW_CAPACITY 8
#endif

typedef struct MiddlewareInfo
{
    MiddlewareHandler *middleware;
    uint16_t middleware_count;
    RequestHandler handler;
} MiddlewareInfo;

extern MiddlewareHandler *global_middleware;
extern uint16_t global_middleware_count;

void chain_start(Req *req, Res *res, MiddlewareInfo *middleware_info);
void reset_middleware(void);
void free_middleware_info(MiddlewareInfo *info);

#endif
