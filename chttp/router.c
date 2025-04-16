#include "router.h"
#include "src/handlers.h"
#include "src/routes.h"
#include <string.h>
#include <stdio.h>

const int route_count = sizeof(routes) / sizeof(Route);

void route_request(SOCKET client_socket, const char *request)
{
    char method[8], path[256];
    const char *body = strstr(request, "\r\n\r\n");
    body = body ? body + 4 : "";

    sscanf(request, "%s %s", method, path);

    Req req = {client_socket, method, path, body};
    Res res = {client_socket, "200 OK", "application/json", NULL};

    for (int i = 0; i < route_count; i++)
    {
        if (strcmp(method, routes[i].method) == 0 && strcmp(path, routes[i].path) == 0)
        {
            routes[i].handler(&req, &res);
            return;
        }
    }

    reply(&res, "404 Not Found", "text/plain", "There is no such route");
}

void reply(Res *res, const char *status, const char *content_type, const char *body)
{
    char response[4096];

    snprintf(response, sizeof(response),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             status, content_type, strlen(body), body);

    send(res->client_socket, response, strlen(response), 0);
}