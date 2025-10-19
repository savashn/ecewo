#ifndef ECEWO_ROUTER_H
#define ECEWO_ROUTER_H

#include "ecewo.h"
#include "uv.h"

typedef struct client_s client_t;

typedef struct
{
    uv_write_t req;
    uv_buf_t buf;
    char *data;
    Arena *arena;
} write_req_t;

typedef struct
{
    const char *method;
    const char *path;
    RequestHandler handler;
    void *middleware_ctx;
} Router;

typedef enum
{
    HANDLER_SYNC = 0,
    HANDLER_ASYNC = 1
} handler_type_t;

extern global_route_trie;

int router(client_t *client, const char *request_data, size_t request_len);
const char *get_req(const request_t *request, const char *key);

#endif
