#include "router.h"
#include "src/handlers.h"
#include "src/routes.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_DYNAMIC_PARAMS 10
#define MAX_QUERY_PARAMS 20

const int route_count = sizeof(routes) / sizeof(Route);

void parse_dynamic_params(const char *path, const char *route_path, params_t *params)
{
    printf("Parsing dynamic params for path: %s, route: %s\n", path, route_path);
    params->count = 0;

    const char *param_start = path;
    const char *route_start = route_path;

    while (*route_start != '\0' && *param_start != '\0')
    {
        if (*route_start == ':' && *(route_start + 1) != '\0')
        {
            const char *key_end = strchr(route_start, '/');
            size_t key_len;

            if (key_end)
            {
                key_len = key_end - route_start - 1;
            }
            else
            {
                key_len = strlen(route_start + 1);
            }

            char *param_key = malloc(key_len + 1);
            strncpy(param_key, route_start + 1, key_len);
            param_key[key_len] = '\0';

            const char *param_end = strchr(param_start, '/');
            size_t param_len = (param_end ? param_end - param_start : strlen(param_start));

            char *param_value = malloc(param_len + 1);
            strncpy(param_value, param_start, param_len);
            param_value[param_len] = '\0';

            if (params->count < MAX_DYNAMIC_PARAMS)
            {
                params->params[params->count].key = param_key;
                params->params[params->count].value = param_value;
                params->count++;
            }

            param_start += param_len;
            if (*param_start == '/')
                param_start++;

            route_start = key_end ? key_end : route_start + strlen(route_start);
            if (*route_start == '/')
                route_start++;
        }
        else
        {
            route_start++;
            param_start++;
        }
    }
}

void parse_query_string(const char *query_string, query_t *query)
{
    query->count = 0;

    if (!query_string || strlen(query_string) == 0)
        return;

    char buffer[1024];
    strncpy(buffer, query_string, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *pair = strtok(buffer, "&");
    while (pair && query->count < MAX_QUERY_PARAMS)
    {
        char *eq = strchr(pair, '=');
        if (eq)
        {
            *eq = '\0';
            query->items[query->count].key = strdup(pair);
            query->items[query->count].value = strdup(eq + 1);
            query->count++;
        }
        pair = strtok(NULL, "&");
    }
}

const char *params_get(params_t *params, const char *key)
{
    for (int i = 0; i < params->count; i++)
    {
        if (strcmp(params->params[i].key, key) == 0)
        {
            return params->params[i].value;
        }
    }
    return NULL;
}

const char *query_get(query_t *query, const char *key)
{
    for (int i = 0; i < query->count; i++)
    {
        if (strcmp(query->items[i].key, key) == 0)
        {
            return query->items[i].value;
        }
    }
    return NULL;
}

// --- ROUTER ---

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

        printf("Checking Route Method: %s\n", route_method);
        printf("Checking Route Path: %s\n", route_path);

        if (strcasecmp(method, route_method) != 0)
        {
            continue;
        }

        params_t dynamic_params = {0};
        dynamic_params.params = malloc(sizeof(param_t) * MAX_DYNAMIC_PARAMS);
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
                .query = parsed_query};

            Res res = {
                .client_socket = client_socket,
                .status = "200 OK",
                .content_type = "application/json",
                .body = NULL};

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
                    .query = parsed_query};

                Res res = {
                    .client_socket = client_socket,
                    .status = "200 OK",
                    .content_type = "application/json",
                    .body = NULL};

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
        .body = NULL};

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
