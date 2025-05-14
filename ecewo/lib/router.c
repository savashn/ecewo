#include <stdlib.h>
#include <string.h>
#include "ecewo.h"
#include "uv.h"
#include "llhttp.h"

void write_completion_cb(uv_write_t *req, int status)
{
    if (status < 0)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(status));
    }

    // Get our write_req_t structure from the uv_write_t pointer
    write_req_t *write_req = (write_req_t *)req;

    // Free the allocated response buffer
    free(write_req->data);

    // Free the write request structure itself
    free(write_req);
}

static void send_error(uv_tcp_t *client_socket, int error_code)
{
    const char *err = NULL;

    if (error_code == 500)
        err = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: 21\r\nConnection: close\r\n\r\nInternal Server Error";
    if (error_code == 400)
        err = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 11\r\nConnection: close\r\n\r\nBad Request";

    // Create a write request structure
    write_req_t *write_req = (write_req_t *)malloc(sizeof(write_req_t));
    if (!write_req)
    {
        fprintf(stderr, "Failed to allocate memory for write request\n");
        return;
    }

    // Allocate memory for the response and copy the bad request message
    char *response = malloc(strlen(err) + 1);
    if (!response)
    {
        fprintf(stderr, "Failed to allocate memory for response\n");
        free(write_req);
        return;
    }

    strcpy(response, err);

    // Store the allocated buffer in the write request
    write_req->data = response;

    // Set up the buffer for libuv
    write_req->buf = uv_buf_init(response, (unsigned int)strlen(response));

    // Send the response asynchronously
    int result = uv_write(&write_req->req, (uv_stream_t *)client_socket, &write_req->buf, 1, write_completion_cb);
    if (result < 0)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(result));
        free(response);
        free(write_req);
    }
}

bool matcher(const char *path, const char *route_path)
{
    if (!path || !route_path)
        return false;

    // Make mutable copies
    char *path_copy = strdup(path);
    char *route_copy = strdup(route_path);
    if (!path_copy || !route_copy)
    {
        free(path_copy);
        free(route_copy);
        return false;
    }

    // Dynamic arrays for segments
    size_t path_cap = 8, route_cap = 8;
    size_t path_segment_count = 0, route_segment_count = 0;
    char **path_segments = malloc(path_cap * sizeof *path_segments);
    char **route_segments = malloc(route_cap * sizeof *route_segments);
    if (!path_segments || !route_segments)
    {
        free(path_copy);
        free(route_copy);
        free(path_segments);
        free(route_segments);
        return false;
    }

    // Split path segments
    char *token = strtok(path_copy, "/");
    while (token)
    {
        if (path_segment_count == path_cap)
        {
            path_cap *= 2;
            char **tmp = realloc(path_segments, path_cap * sizeof *tmp);
            if (!tmp)
                break;
            path_segments = tmp;
        }
        path_segments[path_segment_count++] = token;
        token = strtok(NULL, "/");
    }

    // Split route segments
    token = strtok(route_copy, "/");
    while (token)
    {
        if (route_segment_count == route_cap)
        {
            route_cap *= 2;
            char **tmp = realloc(route_segments, route_cap * sizeof *tmp);
            if (!tmp)
                break;
            route_segments = tmp;
        }
        route_segments[route_segment_count++] = token;
        token = strtok(NULL, "/");
    }

    bool match = true;

    // Compare counts
    if (path_segment_count != route_segment_count)
    {
        match = false;
    }
    else
    {
        // Compare each segment
        for (size_t i = 0; i < path_segment_count; i++)
        {
            const char *rseg = route_segments[i];
            const char *pseg = path_segments[i];

            // If route segment starts with ':', it's a parameter and always matches
            if (rseg[0] == ':')
            {
                continue; // Parameter always matches, move to next segment
            }
            // Static segment check
            if (strcmp(rseg, pseg) != 0)
            {
                match = false; // If static segment doesn't match, route doesn't match
                break;
            }
        }
    }

    free(path_copy);
    free(route_copy);
    free(path_segments);
    free(route_segments);

    // If all segments match, route matches
    return match;
}

// Extract URL path and query parts from the URL
static int extract_path_and_query(const char *url, char **path, char **query)
{
    if (!url || !path || !query)
        return -1;

    // Initialize output pointers to NULL
    *path = NULL;
    *query = NULL;

    // Skip any "http://" or "https://" prefix
    const char *url_path = url;
    if (strncmp(url, "http://", 7) == 0)
    {
        url_path = strchr(url + 7, '/');
        if (!url_path)
            url_path = url + strlen(url);
    }
    else if (strncmp(url, "https://", 8) == 0)
    {
        url_path = strchr(url + 8, '/');
        if (!url_path)
            url_path = url + strlen(url);
    }

    // If URL doesn't start with '/', it might contain hostname
    if (url_path[0] != '/' && url_path == url)
    {
        // Look for first '/' after hostname
        url_path = strchr(url, '/');
        if (!url_path)
        {
            // No path specified, use root
            *path = strdup("/");
            if (!*path)
                return -1; // Memory allocation failed

            *query = strdup("");
            if (!*query)
            {
                free(*path);
                *path = NULL;
                return -1; // Memory allocation failed
            }
            return 0;
        }
    }

    // Find the query part starting with '?'
    const char *query_start = strchr(url_path, '?');

    if (query_start)
    {
        // Allocate and copy path part (without query)
        size_t path_len = query_start - url_path;
        *path = (char *)malloc(path_len + 1);
        if (!*path)
            return -1; // Memory allocation failed

        strncpy(*path, url_path, path_len);
        (*path)[path_len] = '\0';

        // Allocate and copy query part (without the '?')
        size_t query_len = strlen(query_start + 1);
        *query = (char *)malloc(query_len + 1);
        if (!*query)
        {
            free(*path);
            *path = NULL;
            return -1; // Memory allocation failed
        }
        strcpy(*query, query_start + 1);
    }
    else
    {
        // No query part, just copy the path
        *path = strdup(url_path);
        if (!*path)
            return -1; // Memory allocation failed

        *query = strdup("");
        if (!*query)
        {
            free(*path);
            *path = NULL;
            return -1; // Memory allocation failed
        }
    }

    // Default to "/" if path is empty
    if ((*path)[0] == '\0')
    {
        free(*path);
        *path = strdup("/");
        if (!*path)
        {
            free(*query);
            *query = NULL;
            return -1; // Memory allocation failed
        }
    }

    return 0; // Success
}

int router(uv_tcp_t *client_socket, const char *request_data, size_t request_len)
{
    if (!request_data || request_len == 0)
    {
        printf("Invalid empty request\n");
        send_error(client_socket, 400);
        return 1; // Close connection for invalid requests
    }

    // Create and initialize HTTP context
    http_context_t context;
    http_context_init(&context);

    // Parse the HTTP request
    enum llhttp_errno err = llhttp_execute(&context.parser, request_data, request_len);

    if (err != HPE_OK)
    {
        fprintf(stderr, "HTTP parsing error: %s\n", llhttp_errno_name(err));
        send_error(client_socket, 400);
        http_context_free(&context);
        return 1; // Close connection for parsing errors
    }

    // Parse the URL to extract path and query
    char *path = NULL;
    char *query = NULL;

    if (extract_path_and_query(context.url, &path, &query) != 0)
    {
        fprintf(stderr, "Memory allocation error during URL parsing\n");
        send_error(client_socket, 500);
        http_context_free(&context);
        return 1; // Close connection for memory errors
    }

    // Debug info
    printf("Request Method: %s\n", context.method);
    printf("Request Path: %s\n", path);
    // printf("Request Query: %s\n", query);

    // Parse query parameters
    parse_query(query, &context.query_params);

    // Route matching
    bool route_found = false;

    // Start loop over routes
    for (int i = 0; i < route_count; i++)
    {
        const char *route_method = routes[i].method;
        const char *route_path = routes[i].path;

        // Compare request method with route method
        if (strcasecmp(context.method, route_method) != 0)
        {
            continue;
        }

        // Check if route matches
        if (!matcher(path, route_path))
        {
            continue;
        }

        route_found = true;

        // Process dynamic parameters
        parse_params(path, route_path, &context.url_params);

        // Prepare request object
        Req req = {
            .client_socket = client_socket,
            .method = context.method,
            .path = path,
            .body = context.body,
            .params = context.url_params,
            .query = context.query_params,
            .headers = context.headers,
        };

        // Prepare response object with defaults
        Res res = {
            .client_socket = client_socket,
            .status = 200,
            .content_type = "application/json",
            .body = NULL,
            .set_cookie = {0},
            .keep_alive = context.keep_alive // Set the keep-alive status
        };

        // Call the handler for this route
        routes[i].handler(&req, &res);

        // Clean up HTTP context
        http_context_free(&context);

        // Return whether to close the connection
        return res.keep_alive ? 0 : 1;
    }

    // If no route matches, return 404
    if (!route_found)
    {
        printf("No matching route found for: %s %s\n", context.method, path);

        Res res = {
            .client_socket = client_socket,
            .status = 404,
            .content_type = "text/plain",
            .body = NULL,
            .set_cookie = {0},
            .keep_alive = context.keep_alive // Set the keep-alive status
        };

        reply(&res, res.status, res.content_type, "404 Not Found");

        // Clean up HTTP context
        http_context_free(&context);

        // Return whether to close the connection
        return res.keep_alive ? 0 : 1;
    }

    // Clean up HTTP context
    http_context_free(&context);

    // Default to closing the connection
    return 1;
}

void reply(Res *res, int status, const char *content_type, const char *body)
{
    // Calculate the total size needed for the response
    size_t body_len = strlen(body);
    size_t cookie_len = strlen(res->set_cookie);
    size_t header_len = 128 + cookie_len;          // Base headers + cookie
    size_t total_len = header_len + body_len + 64; // Extra padding for safety

    // Allocate memory for the full response
    char *response = malloc(total_len);
    if (!response)
    {
        fprintf(stderr, "Failed to allocate memory for response\n");
        return;
    }

    // Determine Connection header value based on keep_alive flag
    const char *connection_value = res->keep_alive ? "keep-alive" : "close";

    // Format the response
    int written = snprintf(response, total_len,
                           "HTTP/1.1 %d\r\n"         // Status code
                           "%s"                      // Set-Cookie header, if any
                           "Content-Type: %s\r\n"    // Content-Type header
                           "Content-Length: %zu\r\n" // Content-Length header
                           "Connection: %s\r\n"      // Connection header
                           "\r\n"                    // Blank line separating headers and body
                           "%s",                     // The response body
                           status,
                           res->set_cookie[0] ? res->set_cookie : "", // Include Set-Cookie if not empty
                           content_type,
                           body_len,         // Content-Length is the exact length of the body
                           connection_value, // "keep-alive" or "close"
                           body);

    if (written < 0 || (size_t)written >= total_len)
    {
        fprintf(stderr, "Response buffer overflow or formatting error\n");
        free(response);
        return;
    }

    // Debug info
    printf("Sending response: %d bytes, status: %d\n", written, status);

    // Create a write request structure
    write_req_t *write_req = (write_req_t *)malloc(sizeof(write_req_t));
    if (!write_req)
    {
        fprintf(stderr, "Failed to allocate memory for write request\n");
        free(response);
        return;
    }

    // Store the allocated buffer in the write request so we can free it later
    write_req->data = response;

    // Set up the buffer for libuv
    write_req->buf = uv_buf_init(response, written);

    // Important: Point req field to the actual uv_write_t structure
    uv_write_t *write_handle = &write_req->req;

    // Send the response asynchronously
    int result = uv_write(write_handle, (uv_stream_t *)res->client_socket, &write_req->buf, 1, write_completion_cb);
    if (result < 0)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(result));
        free(response);
        free(write_req);
    }

    // Reset the cookie header for the next request
    res->set_cookie[0] = '\0';
}

void set_cookie(Res *res, const char *name, const char *value, int max_age)
{
    if (!name || !value)
    {
        printf("Error: NULL cookie name or value\n");
        return;
    }

    // Safety checks for name and value
    if (strlen(name) > 64 || strlen(value) > 128)
    {
        printf("Error: Cookie name or value too long\n");
        return;
    }

    snprintf(res->set_cookie, sizeof(res->set_cookie),
             "Set-Cookie: %s=%s; Max-Age=%d; HttpOnly\r\n", // Set-Cookie header format
             name, value, max_age);                         // Set the cookie's name, value, and max age
}
