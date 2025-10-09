#ifndef ECEWO_ROUTER_H
#define ECEWO_ROUTER_H

#include "ecewo.h"
#include "uv.h"

// Forward declarations
typedef struct client_s client_t;

// Context entry
typedef struct
{
    char *key;
    void *data;
    size_t size;
} context_entry_t;

// Context
typedef struct
{
    context_entry_t *entries;
    uint32_t count;
    uint32_t capacity;
    Arena *arena;
} context_t;

// Request item
typedef struct
{
    char *key;
    char *value;
} request_item_t;

// Request structure
typedef struct
{
    request_item_t *items;
    uint16_t count;
    uint16_t capacity;
} request_t;

// Request structure
struct Req
{
    Arena *arena;
    uv_tcp_t *client_socket;
    char *method;
    char *path;
    char *body;
    size_t body_len;
    request_t headers;
    request_t query;
    request_t params;
    context_t ctx; // Middleware context
};

// HTTP Header structure
typedef struct
{
    char *name;
    char *value;
} http_header_t;

// Response structure
struct Res
{
    Arena *arena;
    uv_tcp_t *client_socket;
    uint16_t status;
    char *content_type;
    void *body;
    size_t body_len;
    bool keep_alive;
    http_header_t *headers;
    uint16_t header_count;
    uint16_t header_capacity;
};

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

// Async handler context for thread pool execution
typedef struct
{
    uv_work_t work_req;
    RequestHandler handler;
    Req *req;
    Res *res;
    bool completed;
    const char *error_message;
} async_handler_context_t;

// Internal functions
int execute_async_handler(RequestHandler handler, Req *req, Res *res);
int router(client_t *client, const char *request_data, size_t request_len);
const char *get_req(const request_t *request, const char *key);

#endif
