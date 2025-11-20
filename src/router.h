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
    void *async_context;
} write_req_t;

typedef enum
{
    HANDLER_SYNC = 0,
    HANDLER_ASYNC = 1
} handler_type_t;

struct task_s
{
    uv_work_t work; // libuv requirement
    void *context;  // User provided context data
    Arena *arena;   // Arena reference for error handling
    char *error;    // libuv error message

    // Task callbacks
    work_handler_t work_fn;     // Work to be done in thread pool
    result_handler_t result_fn; // Result handler
};

int router(client_t *client, const char *request_data, size_t request_len);
const char *get_req(const request_t *request, const char *key);

#endif
