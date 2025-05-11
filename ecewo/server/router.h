#ifndef ROUTER_H
#define ROUTER_H
#include "request.h"
#include "uv.h"
#include <stdbool.h>

typedef struct
{
    uv_tcp_t *client_socket;
    const char *method;
    const char *path;
    char *body;
    request_t headers;
    request_t query;
    request_t params;
} Req;

typedef struct
{
    uv_tcp_t *client_socket;
    char *status;
    char *content_type;
    char *body;
    char set_cookie[256];
    int keep_alive;
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
// int router(uv_tcp_t *client_socket, const char *request);
int router(uv_tcp_t *client_socket, const char *request_data, size_t request_len);

void reply(Res *res, const char *status, const char *content_type, const char *body);
void set_cookie(Res *res, const char *name, const char *value, int max_age);

#endif
