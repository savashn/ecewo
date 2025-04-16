#ifndef ROUTER_H
#define ROUTER_H
#include <winsock2.h>

typedef void (*RequestHandler)(SOCKET client_socket, const char *body);
void route_request(SOCKET client_socket, const char *request);

typedef struct
{
    const char *method;
    const char *path;
    RequestHandler handler;
} Route;

#endif
