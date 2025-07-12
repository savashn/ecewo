#ifndef ECEWO_H
#define ECEWO_H

#include <stdio.h>
#include <stdlib.h>
#include "router.h"
#include "middleware.h"
#include "compat.h"

extern Router *routes;
extern size_t route_count;
extern size_t routes_capacity;

void expand_routes(void);

// Common helper
static inline void register_route_simple(
    const char *method,
    const char *path,
    MiddlewareArray mw,
    RequestHandler handler)
{
    register_route(method, path, mw, handler);
}

// Chooser for macro overloading
#define GET_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define get(...) \
    GET_CHOOSER(__VA_ARGS__, get_with_mw, get_no_mw)(__VA_ARGS__)

static inline void get_no_mw(const char *path, RequestHandler handler)
{
    register_route_simple("GET", path, NO_MW, handler);
}

static inline void get_with_mw(
    const char *path,
    MiddlewareArray mw,
    RequestHandler handler)
{
    register_route_simple("GET", path, mw, handler);
}

#define POST_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define post(...) \
    POST_CHOOSER(__VA_ARGS__, post_with_mw, post_no_mw)(__VA_ARGS__)
static inline void post_no_mw(const char *p, RequestHandler h)
{
    register_route_simple("POST", p, NO_MW, h);
}
static inline void post_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_route_simple("POST", p, mw, h);
}

#define PUT_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define put(...) \
    PUT_CHOOSER(__VA_ARGS__, put_with_mw, put_no_mw)(__VA_ARGS__)
static inline void put_no_mw(const char *p, RequestHandler h)
{
    register_route_simple("PUT", p, NO_MW, h);
}
static inline void put_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_route_simple("PUT", p, mw, h);
}

#define DEL_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define del(...) \
    DEL_CHOOSER(__VA_ARGS__, del_with_mw, del_no_mw)(__VA_ARGS__)
static inline void del_no_mw(const char *p, RequestHandler h)
{
    register_route_simple("DELETE", p, NO_MW, h);
}
static inline void del_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_route_simple("DELETE", p, mw, h);
}

#endif
