#ifndef HANDLER_H
#define HANDLER_H
#include "request.h"

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

typedef struct
{
    socket_t client_socket;
    const char *method;
    const char *path;
    char *body;
    request_t headers;
    request_t query;
    request_t params;
} Req;

typedef struct
{
    socket_t client_socket;
    char *status;
    char *content_type;
    char *body;
    char set_cookie[256];
    int keep_alive;
} Res;

typedef void (*RequestHandler)(Req *req, Res *res);

typedef struct
{
    const char *method;
    const char *path;
    RequestHandler handler;
    void *middleware_ctx; // for middleware support
} Router;

// Returns 1 if connection should be closed, 0 if it should stay open
int router(socket_t client_socket, const char *request);

void reply(Res *res, const char *status, const char *content_type, const char *body);
void set_cookie(Res *res, const char *name, const char *value, int max_age);

#endif
