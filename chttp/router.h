#ifndef ROUTER_H
#define ROUTER_H
#include <winsock2.h>

typedef struct
{
    SOCKET client_socket;
    const char *method;
    const char *path;
    const char *body;
} Req;

typedef struct
{
    SOCKET client_socket;
} Res;

typedef void (*RequestHandler)(Req *req, Res *res);

typedef struct
{
    const char *method;
    const char *path;
    RequestHandler handler;
} Route;

void route_request(SOCKET client_socket, const char *request);

#endif
