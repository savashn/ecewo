#ifndef ROUTER_H
#define ROUTER_H
#include "request.h"
#include "uv.h"
#include <stdbool.h>

// Context structure for middleware data
typedef struct
{
    void *data;
    size_t size;
    void (*cleanup)(void *data);
} req_context_t;

typedef struct
{
    uv_tcp_t *client_socket;
    const char *method;
    const char *path;
    char *body;
    size_t body_len;
    request_t headers;
    request_t query;
    request_t params;
    req_context_t context;
} Req;

// HTTP Header structure
typedef struct
{
    char *name;
    char *value;
} http_header_t;

typedef struct
{
    uv_tcp_t *client_socket;
    int status;
    char *content_type;
    void *body;
    size_t body_len;
    int keep_alive;
    http_header_t *headers; // Dynamic header array
    int header_count;
    int header_capacity;
} Res;

typedef struct
{
    uv_write_t req;
    uv_buf_t buf;
    char *data;
    Res *res;
} write_req_t;

typedef void (*RequestHandler)(Req *req, Res *res);

typedef struct
{
    const char *method;
    const char *path;
    RequestHandler handler;
    void *middleware_ctx;
} Router;

bool matcher(const char *path, const char *route_path);

// Returns 1 if connection should be closed, 0 if it should stay open
int router(uv_tcp_t *client_socket, const char *request_data, size_t request_len);

void set_header(Res *res, const char *name, const char *value);

// Context management functions
void set_context(Req *req, void *data, size_t size, void (*cleanup)(void *));
void *get_context(Req *req);

void reply(Res *res, int status, const char *content_type, const void *body, size_t body_len);

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

Req *copy_req(const Req *original);
Res *copy_res(const Res *original);
void destroy_req(Req *req);
void destroy_res(Res *res);

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
