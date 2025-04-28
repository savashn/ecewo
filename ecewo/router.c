#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "router.h"
#include "request.h"
#include "routes.h"

#define MAX_DYNAMIC_PARAMS 20

bool matcher(const char *path, const char *route_path)
{
    // Create temporary copies to split paths by '/' character
    char path_copy[256];
    char route_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    strncpy(route_copy, route_path, sizeof(route_copy) - 1);
    route_copy[sizeof(route_copy) - 1] = '\0';

    // Arrays to hold path segments
    char *path_segments[20];
    char *route_segments[20];
    int path_segment_count = 0;
    int route_segment_count = 0;

    // Split path segments
    char *token = strtok(path_copy, "/");
    while (token != NULL && path_segment_count < 20)
    {
        path_segments[path_segment_count++] = token;
        token = strtok(NULL, "/");
    }

    // Split route segments
    token = strtok(route_copy, "/");
    while (token != NULL && route_segment_count < 20)
    {
        route_segments[route_segment_count++] = token;
        token = strtok(NULL, "/");
    }

    // If segment counts differ, route doesn't match
    if (path_segment_count != route_segment_count)
    {
        printf("Route segment count mismatch: %d vs %d\n",
               path_segment_count, route_segment_count);
        return false;
    }

    // Compare segments
    for (int i = 0; i < path_segment_count; i++)
    {
        // If route segment starts with ':', it's a parameter and always matches
        if (route_segments[i][0] == ':')
        {
            continue; // Parameter always matches, move to next segment
        }
        // Static segment check
        else if (strcmp(route_segments[i], path_segments[i]) != 0)
        {
            printf("Static segment mismatch at position %d: '%s' vs '%s'\n",
                   i, route_segments[i], path_segments[i]);
            return false; // If static segment doesn't match, route doesn't match
        }
    }

    // If all segments match, route matches
    printf("Route matches: %s\n", route_path);
    return true;
}

void router(SOCKET client_socket, const char *request)
{
    char method[8], full_path[256], path[256], query[256];
    const char *body_start = strstr(request, "\r\n\r\n");
    size_t body_length = 0;
    char *body = NULL;

    if (body_start)
    {
        body_start += 4; // Skip the "\r\n\r\n"
        body_length = strlen(body_start);
        body = malloc(body_length + 1);
        if (body)
        {
            memcpy(body, body_start, body_length);
            body[body_length] = '\0'; // null-terminate
        }
    }

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

    request_t parsed_query = {0};
    parse_query(query, &parsed_query);

    request_t headers = {0};
    parse_headers(request, &headers);

    // Start loop over routes
    for (int i = 0; i < route_count; i++)
    {
        const char *route_method = routes[i].method;
        const char *route_path = routes[i].path;

        // Compare request method with route method
        if (strcasecmp(method, route_method) != 0)
        {
            continue;
        }

        // Check if route matches
        if (!matcher(path, route_path))
        {
            continue;
        }

        // Set up parameter array for dynamic parameters
        request_t params = {0};
        params.items = malloc(sizeof(*params.items) * MAX_DYNAMIC_PARAMS);
        if (params.items == NULL)
        {
            printf("Memory allocation failed for dynamic params\n");
            return;
        }

        // Process dynamic parameters
        parse_params(path, route_path, &params);

        printf("Dynamic Params Found:\n");
        for (int j = 0; j < params.count; j++)
        {
            printf("Key: %s, Value: %s\n", params.items[j].key, params.items[j].value);
        }

        Req req = {
            .client_socket = client_socket,
            .method = method,
            .path = path,
            .body = body,
            .params = params,
            .query = parsed_query,
            .headers = headers,
        };

        Res res = {
            .client_socket = client_socket,
            .status = "200 OK",
            .content_type = "application/json",
            .body = NULL,
        };

        routes[i].handler(&req, &res);

        if (body)
            free(body);

        if (parsed_query.items && parsed_query.count > 0)
            free_req(&parsed_query);

        if (headers.items && headers.count > 0)
            free_req(&headers);

        if (params.items && params.count > 0)
            free_req(&params);

        return;
    }

    // If no route matches, return 404
    printf("No matching route found\n");

    Res res = {
        .client_socket = client_socket,
        .status = "404",
        .content_type = "text/plain",
        .body = NULL,
    };

    reply(&res, res.status, res.content_type, "404 Not Found");

    free_req(&parsed_query);
    free_req(&headers);
    free(body);
}

void reply(Res *res, const char *status, const char *content_type, const char *body)
{
    char response[4096];

    snprintf(response, sizeof(response),
             "HTTP/1.1 %s\r\n"         // Status code and status text
             "%s"                      // Set-Cookie header, if any (if set_cookie is not empty, it is included)
             "Content-Type: %s\r\n"    // Content-Type header
             "Content-Length: %lu\r\n" // Content-Length header (length of the body)
             "Connection: close\r\n"   // Connection header, indicating the connection will be closed after the response
             "\r\n"                    // Blank line separating headers and body
             "%s",                     // The response body
             status,
             res->set_cookie[0] ? res->set_cookie : "", // Include the Set-Cookie header if not empty
             content_type,
             strlen(body), // Content-Length is the length of the body
             body);

    send(res->client_socket, response, strlen(response), 0);

    res->set_cookie[0] = '\0'; // Reset the cookie header for the next request
}

void set_cookie(Res *res, const char *name, const char *value, int max_age)
{
    snprintf(res->set_cookie, sizeof(res->set_cookie),
             "Set-Cookie: %s=%s; Max-Age=%d; HttpOnly\r\n", // Set-Cookie header format
             name, value, max_age);                         // Set the cookie's name, value, and max age
}
