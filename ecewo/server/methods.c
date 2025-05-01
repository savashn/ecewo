#include <stdio.h>
#include "ecewo.h"

Router routes[MAX_ROUTES];
int route_count = 0;

void get(const char *path, RequestHandler handler)
{
    if (route_count >= MAX_ROUTES)
    {
        printf("Maximum route count reached. Cannot add more routes.\n");
        return;
    }

    routes[route_count].method = "GET";
    routes[route_count].path = path;
    routes[route_count].handler = handler;
    route_count++;
}

void post(const char *path, RequestHandler handler)
{
    if (route_count >= MAX_ROUTES)
    {
        printf("Maximum route count reached. Cannot add more routes.\n");
        return;
    }

    routes[route_count].method = "POST";
    routes[route_count].path = path;
    routes[route_count].handler = handler;
    route_count++;
}

void put(const char *path, RequestHandler handler)
{
    if (route_count >= MAX_ROUTES)
    {
        printf("Maximum route count reached. Cannot add more routes.\n");
        return;
    }

    routes[route_count].method = "PUT";
    routes[route_count].path = path;
    routes[route_count].handler = handler;
    route_count++;
}

void del(const char *path, RequestHandler handler)
{
    if (route_count >= MAX_ROUTES)
    {
        printf("Maximum route count reached. Cannot add more routes.\n");
        return;
    }

    routes[route_count].method = "DELETE";
    routes[route_count].path = path;
    routes[route_count].handler = handler;
    route_count++;
}
