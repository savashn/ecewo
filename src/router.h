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
    uv_work_t work;
    uv_async_t async_send;
    void *context;
    spawn_handler_t work_fn;
    spawn_handler_t result_fn;
} spawn_t;

int router(client_t *client, const char *request_data, size_t request_len);
const char *get_req(const request_t *request, const char *key);

#endif
