#ifndef ROUTER_H
#define ROUTER_H

#include "request.h"
#include "uv.h"
#include <stdbool.h>

// Forward declarations
typedef struct Req Req;
typedef struct Res Res;

// Context structure for middleware data
typedef struct
{
    void *data;
    size_t size;
    void (*cleanup)(void *data);
} req_context_t;

// Request structure
typedef struct Req
{
    uv_tcp_t *client_socket;
    char *method;
    char *path;
    char *body;
    size_t body_len;
    request_t headers;
    request_t query;
    request_t params;
    req_context_t context; // Middleware context
} Req;

// HTTP Header structure
typedef struct
{
    char *name;
    char *value;
} http_header_t;

// Response structure
typedef struct Res
{
    uv_tcp_t *client_socket;
    int status;
    char *content_type; // Static string, not owned
    void *body;         // Not owned by Res
    size_t body_len;
    int keep_alive;
    http_header_t *headers; // Heap allocated array
    int header_count;
    int header_capacity;
} Res;

// Write request structure
typedef struct
{
    uv_write_t req;
    uv_buf_t buf;
    char *data; // Heap allocated
} write_req_t;

// Route handler function type
typedef void (*RequestHandler)(Req *req, Res *res);

// Route definition
typedef struct
{
    const char *method;
    const char *path;
    RequestHandler handler;
    void *middleware_ctx;
} Router;

bool matcher(const char *path, const char *route_path);
int router(uv_tcp_t *client_socket, const char *request_data, size_t request_len);
Req *copy_req(const Req *original);
Res *copy_res(const Res *original);
void destroy_req(Req *req);
void destroy_res(Res *res);

void set_header(Res *res, const char *name, const char *value);
void reply(Res *res, int status, const char *content_type, const void *body, size_t body_len);

// Context management functions
void set_context(Req *req, void *data, size_t size, void (*cleanup)(void *));
void *get_context(Req *req);

// Convenience response functions
static inline void send_text(Res *res, int status, const char *body)
{
    reply(res, status, "text/plain", body, strlen(body));
}

static inline void send_html(Res *res, int status, const char *body)
{
    reply(res, status, "text/html", body, strlen(body));
}

static inline void send_json(Res *res, int status, const char *body)
{
    reply(res, status, "application/json", body, strlen(body));
}

static inline void send_cbor(Res *res, int status, const char *body, size_t body_len)
{
    reply(res, status, "application/cbor", body, body_len);
}

// Convenience getter functions
static inline const char *get_params(const Req *req, const char *key)
{
    return get_req(&req->params, key);
}

static inline const char *get_query(const Req *req, const char *key)
{
    return get_req(&req->query, key);
}

static inline const char *get_headers(const Req *req, const char *key)
{
    return get_req(&req->headers, key);
}

#endif
