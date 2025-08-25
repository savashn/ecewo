#ifndef ECEWO_H
#define ECEWO_H

#include "../src/lib/router.h"
#include "../src/lib/middleware.h"
#include "../src/lib/route_trie.h"
#include "../src/config/compat.h"
#include "../src/utils/cookie.h"

extern route_trie_t *global_route_trie;

// GET
#define GET_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define get(...) \
    GET_CHOOSER(__VA_ARGS__, get_with_mw, get_no_mw)(__VA_ARGS__)

static inline void get_no_mw(const char *path, RequestHandler handler)
{
    register_route("GET", path, NO_MW, handler);
}

static inline void get_with_mw(
    const char *path,
    MiddlewareArray mw,
    RequestHandler handler)
{
    register_route("GET", path, mw, handler);
}

// POST
#define POST_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define post(...) \
    POST_CHOOSER(__VA_ARGS__, post_with_mw, post_no_mw)(__VA_ARGS__)

static inline void post_no_mw(const char *p, RequestHandler h)
{
    register_route("POST", p, NO_MW, h);
}

static inline void post_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_route("POST", p, mw, h);
}

// PUT
#define PUT_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define put(...) \
    PUT_CHOOSER(__VA_ARGS__, put_with_mw, put_no_mw)(__VA_ARGS__)

static inline void put_no_mw(const char *p, RequestHandler h)
{
    register_route("PUT", p, NO_MW, h);
}

static inline void put_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_route("PUT", p, mw, h);
}

// PATCH
#define PATCH_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define patch(...) \
    PATCH_CHOOSER(__VA_ARGS__, patch_with_mw, patch_no_mw)(__VA_ARGS__)

static inline void patch_no_mw(const char *p, RequestHandler h)
{
    register_route("PATCH", p, NO_MW, h);
}

static inline void patch_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_route("PATCH", p, mw, h);
}

// DELETE
#define DEL_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define del(...) \
    DEL_CHOOSER(__VA_ARGS__, del_with_mw, del_no_mw)(__VA_ARGS__)

static inline void del_no_mw(const char *p, RequestHandler h)
{
    register_route("DELETE", p, NO_MW, h);
}

static inline void del_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_route("DELETE", p, mw, h);
}

#endif
