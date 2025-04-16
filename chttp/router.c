#include "router.h"
#include "../src/handlers.h"
#include "utils.h"
#include "../src/routes.h"
#include <string.h>
#include <stdio.h>

const int route_count = sizeof(routes) / sizeof(Route);

void route_request(SOCKET client_socket, const char *request)
{
    char method[8], path[256];
    const char *body = strstr(request, "\r\n\r\n");
    body = body ? body + 4 : "";

    sscanf(request, "%s %s", method, path);

    for (int i = 0; i < route_count; i++)
    {
        if (strcmp(method, routes[i].method) == 0 && strcmp(path, routes[i].path) == 0)
        {
            routes[i].handler(client_socket, body);
            return;
        }
    }

    res(client_socket, "404 Not Found", "text/plain", "There is no such route");
}
