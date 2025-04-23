#ifndef ROUTER_H
#define ROUTER_H
#include <winsock2.h>
#include "utils/request.h"

typedef struct
{
    SOCKET client_socket;
    const char *method;
    const char *path;
    const char *body;
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

#endif
