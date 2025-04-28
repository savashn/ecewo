#ifndef ROUTER_H
#define ROUTER_H
#include <winsock2.h>
#include "request.h"

typedef struct
{
    SOCKET client_socket;
    const char *method;
    const char *path;
    char *body;
    request_t headers;
    request_t query;
    request_t params;
} Req;

typedef struct
{
    SOCKET client_socket;
    char *status;
    char *content_type;
    char *body;
    char set_cookie[256];
} Res;

typedef void (*RequestHandler)(Req *req, Res *res);

typedef struct
{
    const char *method;
    const char *path;
    RequestHandler handler;
} Router;

void router(SOCKET client_socket, const char *request);
void reply(Res *res, const char *status, const char *content_type, const char *body);
void set_cookie(Res *res, const char *name, const char *value, int max_age);

#endif
