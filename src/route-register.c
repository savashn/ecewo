#include <stdarg.h>
#include <stdlib.h>
#include "route-trie.h"
#include "middleware.h"

#define MAX_STACK_MW 8

#define ROUTE_REGISTER(func_name, method_enum)                                            \
    void func_name(const char *path, int mw_count, ...)                                   \
    {                                                                                     \
        if (!path)                                                                        \
        {                                                                                 \
            LOG_ERROR("NULL path in route registration");                                 \
            return;                                                                       \
        }                                                                                 \
                                                                                          \
        va_list args;                                                                     \
        va_start(args, mw_count);                                                         \
                                                                                          \
        MiddlewareHandler stack_mw[MAX_STACK_MW];                                         \
        MiddlewareHandler *mw = NULL;                                                     \
        if (mw_count > 0)                                                                 \
        {                                                                                 \
            if (mw_count <= MAX_STACK_MW)                                                 \
            {                                                                             \
                mw = stack_mw;                                                            \
            }                                                                             \
            else                                                                          \
            {                                                                             \
                mw = malloc(sizeof(MiddlewareHandler) * mw_count);                        \
                if (!mw)                                                                  \
                {                                                                         \
                    LOG_ERROR("Middleware allocation failed");                            \
                    va_end(args);                                                         \
                    return;                                                               \
                }                                                                         \
            }                                                                             \
                                                                                          \
            for (int i = 0; i < mw_count; i++)                                            \
            {                                                                             \
                mw[i] = va_arg(args, MiddlewareHandler);                                  \
                if (!mw[i])                                                               \
                {                                                                         \
                    LOG_ERROR("NULL middleware handler at index %d", i);                  \
                    if (mw_count > MAX_STACK_MW) free(mw);                                \
                    va_end(args);                                                         \
                    return;                                                               \
                }                                                                         \
            }                                                                             \
        }                                                                                 \
                                                                                          \
        RequestHandler handler = va_arg(args, RequestHandler);                            \
        va_end(args);                                                                     \
                                                                                          \
        if (!handler)                                                                     \
        {                                                                                 \
            LOG_ERROR("NULL handler in route registration");                              \
            if (mw_count > MAX_STACK_MW)                                                  \
                free(mw);                                                                 \
            return;                                                                       \
        }                                                                                 \
                                                                                          \
        MiddlewareInfo *info = calloc(1, sizeof(MiddlewareInfo));                         \
        if (!info)                                                                        \
        {                                                                                 \
            if (mw_count > MAX_STACK_MW)                                                  \
                free(mw);                                                                 \
            return;                                                                       \
        }                                                                                 \
                                                                                          \
        info->handler = handler;                                                          \
        info->middleware_count = mw_count;                                                \
                                                                                          \
        if (mw_count > 0 && mw_count <= MAX_STACK_MW)                                     \
        {                                                                                 \
            info->middleware = malloc(sizeof(MiddlewareHandler) * mw_count);              \
            if (!info->middleware)                                                        \
            {                                                                             \
                free(info);                                                               \
                return;                                                                   \
            }                                                                             \
            memcpy(info->middleware, mw, sizeof(MiddlewareHandler) * mw_count);           \
        }                                                                                 \
        else                                                                              \
        {                                                                                 \
            info->middleware = mw;                                                        \
        }                                                                                 \
                                                                                          \
        int result = route_trie_add(global_route_trie, method_enum, path, handler, info); \
        if (result != 0)                                                                  \
        {                                                                                 \
            LOG_ERROR("Failed to add route: %s", path);                                   \
            free_middleware_info(info);                                                   \
        }                                                                                 \
    }

ROUTE_REGISTER(register_get, HTTP_GET)
ROUTE_REGISTER(register_post, HTTP_POST)
ROUTE_REGISTER(register_put, HTTP_PUT)
ROUTE_REGISTER(register_patch, HTTP_PATCH)
ROUTE_REGISTER(register_del, HTTP_DELETE)
ROUTE_REGISTER(register_head, HTTP_HEAD)
ROUTE_REGISTER(register_options, HTTP_OPTIONS)
