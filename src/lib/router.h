#ifndef ECEWO_ROUTER_H
#define ECEWO_ROUTER_H

#include "ecewo.h"
#include "uv.h"

// Forward declarations
typedef struct client_s client_t;

// Write request structure (managed by libuv)
typedef struct
{
    uv_write_t req;
    uv_buf_t buf;
    char *data; // Heap allocated (managed by libuv callbacks)
    Arena *arena;
} write_req_t;

// Route definition
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

// Internal functions
int router(client_t *client, const char *request_data, size_t request_len);
const char *get_req(const request_t *request, const char *key);

#endif
