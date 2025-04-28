#include "routes.h"
#include <string.h>

Router routes[MAX_ROUTES];
int route_count = 0;

void add_route(const char *method, const char *path, RequestHandler handler)
{
    if (route_count >= MAX_ROUTES)
        return;
    routes[route_count].method = method;
    routes[route_count].path = path;
    routes[route_count].handler = handler;
    route_count++;
}

void get(const char *path, RequestHandler handler)
{
    add_route("GET", path, handler);
}

void post(const char *path, RequestHandler handler)
{
    add_route("POST", path, handler);
}

void put(const char *path, RequestHandler handler)
{
    add_route("PUT", path, handler);
}

void del(const char *path, RequestHandler handler)
{
    add_route("DELETE", path, handler);
}
