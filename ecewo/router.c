#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "router.h"
#include "utils/params.h"
#include "utils/query.h"
#include "src/routes.h"

const int route_count = sizeof(routes) / sizeof(Router);

void router(SOCKET client_socket, const char *request)
{

    char method[8], full_path[256], path[256], query[256];
    const char *body = strstr(request, "\r\n\r\n");
    body = body ? body + 4 : "";

    sscanf(request, "%s %s", method, full_path);

    printf("Request Method: %s\n", method);
    printf("Request Path: %s\n", full_path);

    char *qmark = strchr(full_path, '?');
    if (qmark)
    {
        strncpy(path, full_path, qmark - full_path);
        path[qmark - full_path] = '\0';
        strcpy(query, qmark + 1);
    }
    else
    {
        strcpy(path, full_path);
        query[0] = '\0';
    }

    printf("Parsed Path: %s\n", path);
    printf("Parsed Query: %s\n", query);

    query_t parsed_query = {0};
    parse_query_string(query, &parsed_query);

    for (int i = 0; i < route_count; i++)
    {
        const char *route_method = routes[i].method;
        const char *route_path = routes[i].path;

        if (strcasecmp(method, route_method) != 0)
        {
            continue;
        }

        params_t dynamic_params = {0};
        dynamic_params.params = malloc(sizeof(params_item_t) * MAX_DYNAMIC_PARAMS);
        if (dynamic_params.params == NULL)
        {
            printf("Memory allocation failed for dynamic params\n");
            return;
        }

        parse_dynamic_params(path, route_path, &dynamic_params);

        if (dynamic_params.count > 0)
        {
            printf("Route found, invoking handler\n");
            printf("Dynamic Params Found:\n");
            for (int i = 0; i < dynamic_params.count; i++)
            {
                printf("Key: %s, Value: %s\n", dynamic_params.params[i].key, dynamic_params.params[i].value);
            }

            Req req = {
                .client_socket = client_socket,
                .method = method,
                .path = path,
                .body = body,
                .params = dynamic_params,
                .query = parsed_query,
            };

            Res res = {
                .client_socket = client_socket,
                .status = "200 OK",
                .content_type = "application/json",
                .body = NULL,
            };

            routes[i].handler(&req, &res);

            for (int j = 0; j < dynamic_params.count; j++)
            {
                free(dynamic_params.params[j].key);
                free(dynamic_params.params[j].value);
            }
            free(dynamic_params.params);

            return;
        }
        else
        {
            if (strcmp(path, route_path) == 0)
            {
                Req req = {
                    .client_socket = client_socket,
                    .method = method,
                    .path = path,
                    .body = body,
                    .params = dynamic_params,
                    .query = parsed_query,
                };

                Res res = {
                    .client_socket = client_socket,
                    .status = "200 OK",
                    .content_type = "application/json",
                    .body = NULL,
                };

                routes[i].handler(&req, &res);
                return;
            }
        }
    }

    printf("No matching route found\n");

    Res res = {
        .client_socket = client_socket,
        .status = "404 Not Found",
        .content_type = "text/plain",
        .body = NULL,
    };

    reply(&res, res.status, res.content_type, "There is no such route");
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
