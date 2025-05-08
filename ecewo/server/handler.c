#include <stdlib.h>
#include <string.h>
// #include <stdbool.h>
#include "router.h"
#include "uv.h"

#define MAX_DYNAMIC_PARAMS 20
#define MAX_PATH_SEGMENTS 30
#define MAX_SEGMENT_LENGTH 128

bool matcher(const char *path, const char *route_path)
{
    // Create temporary copies to split paths by '/' character
    char path_copy[512];
    char route_copy[512];

    // Safety check for path lengths
    if (strlen(path) >= sizeof(path_copy) || strlen(route_path) >= sizeof(route_copy))
    {
        printf("Path too long: %s or %s\n", path, route_path);
        return false;
    }

    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    strncpy(route_copy, route_path, sizeof(route_copy) - 1);
    route_copy[sizeof(route_copy) - 1] = '\0';

    // Arrays to hold path segments
    char *path_segments[MAX_PATH_SEGMENTS];
    char *route_segments[MAX_PATH_SEGMENTS];
    int path_segment_count = 0;
    int route_segment_count = 0;

    // Split path segments
    char *token = strtok(path_copy, "/");
    while (token != NULL && path_segment_count < MAX_PATH_SEGMENTS)
    {
        path_segments[path_segment_count++] = token;
        token = strtok(NULL, "/");
    }

    // Split route segments
    token = strtok(route_copy, "/");
    while (token != NULL && route_segment_count < MAX_PATH_SEGMENTS)
    {
        route_segments[route_segment_count++] = token;
        token = strtok(NULL, "/");
    }

    // If segment counts differ, route doesn't match
    if (path_segment_count != route_segment_count)
    {
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
            return false; // If static segment doesn't match, route doesn't match
        }
    }

    // If all segments match, route matches
    return true;
}

// Check if the request has "Connection: keep-alive" header
static int has_keepalive_header(const request_t *headers)
{
    for (int i = 0; i < headers->count; i++)
    {
        if (strcasecmp(headers->items[i].key, "Connection") == 0)
        {
            if (strcasecmp(headers->items[i].value, "keep-alive") == 0)
            {
                return 1;
            }
            else if (strcasecmp(headers->items[i].value, "close") == 0)
            {
                return 0;
            }
        }
    }
    return -1; // No Connection header found
}

// Determine if the connection should be kept alive based on HTTP version and headers
static int should_keep_alive(const char *request, const request_t *headers)
{
    // Check HTTP version
    if (strstr(request, "HTTP/1.1"))
    {
        // HTTP/1.1 default is keep-alive
        int header_value = has_keepalive_header(headers);
        if (header_value == 0)
        {
            // Connection: close was specified
            return 0;
        }
        // Either Connection: keep-alive or no Connection header (default to keep-alive for HTTP/1.1)
        return 1;
    }
    else
    {
        // HTTP/1.0 or other (default to close)
        int header_value = has_keepalive_header(headers);
        if (header_value == 1)
        {
            // Connection: keep-alive was specified
            return 1;
        }
        // Either Connection: close or no Connection header (default to close for HTTP/1.0)
        return 0;
    }
}

int router(uv_tcp_t *client_socket, const char *request)
{
    if (!request || !*request)
    {
        printf("Invalid empty request\n");
        return 1; // Close connection for invalid requests
    }

    char method[16] = {0};
    char full_path[512] = {0};
    char path[512] = {0};
    char query[512] = {0};

    // Parse the request line
    if (sscanf(request, "%15s %511s", method, full_path) != 2)
    {
        printf("Invalid request format\n");

        // 400 Bad Request message
        const char *bad_request = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 11\r\nConnection: close\r\n\r\nBad Request";

        uv_write_t req; // Write request
        uv_buf_t buf = uv_buf_init((char *)bad_request, (unsigned int)strlen(bad_request));

        // Send the response asynchronously using uv_write
        uv_write(&req, (uv_stream_t *)&client_socket, &buf, 1, NULL);

        return 1; // Close connection for invalid requests
    }

    // Find body - it starts after \r\n\r\n
    const char *body_start = strstr(request, "\r\n\r\n");
    size_t body_length = 0;
    char *body = NULL;

    if (body_start)
    {
        body_start += 4; // Skip the "\r\n\r\n"
        body_length = strlen(body_start);

        // Allocate memory for body and copy data
        body = malloc(body_length + 1);
        if (body)
        {
            memcpy(body, body_start, body_length);
            body[body_length] = '\0'; // null-terminate
        }
        else
        {
            printf("Failed to allocate memory for request body\n");
        }
    }

    // Parse path and query
    char *qmark = strchr(full_path, '?');
    if (qmark)
    {
        size_t path_len = qmark - full_path;
        if (path_len >= sizeof(path))
        {
            path_len = sizeof(path) - 1;
        }

        strncpy(path, full_path, path_len);
        path[path_len] = '\0';

        strncpy(query, qmark + 1, sizeof(query) - 1);
        query[sizeof(query) - 1] = '\0';
    }
    else
    {
        strncpy(path, full_path, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        query[0] = '\0';
    }

    // Debug info
    printf("Request Method: %s\n", method);
    printf("Request Path: %s\n", path);
    printf("Request Query: %s\n", query);

    // Initialize query and headers to prevent issues with freeing uninitialized memory
    request_t parsed_query = {0};
    request_t headers = {0};

    // Parse query parameters
    parse_query(query, &parsed_query);

    // Parse headers
    parse_headers(request, &headers);

    // Determine if the connection should be kept alive
    int keep_alive = should_keep_alive(request, &headers);

    // Route matching
    bool route_found = false;

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

        route_found = true;

        // Set up parameter array for dynamic parameters
        request_t params = {0};
        params.items = malloc(sizeof(*params.items) * MAX_DYNAMIC_PARAMS);
        if (params.items == NULL)
        {
            printf("Memory allocation failed for dynamic params\n");

            if (body)
            {
                free(body);
                body = NULL;
            }

            free_req(&parsed_query);
            free_req(&headers);
            return 1; // Close connection on error
        }

        // Process dynamic parameters
        printf("Parsing dynamic params for path: %s, route: %s\n", path, route_path);
        parse_params(path, route_path, &params);

        // Debug info for params
        printf("Dynamic Params Found: %d\n", params.count);
        for (int j = 0; j < params.count; j++)
        {
            printf("  Key: %s, Value: %s\n", params.items[j].key, params.items[j].value);
        }

        // Prepare request object
        Req req = {
            .client_socket = client_socket,
            .method = method,
            .path = path,
            .body = body,
            .params = params,
            .query = parsed_query,
            .headers = headers,
        };

        // Prepare response object with defaults
        Res res = {
            .client_socket = client_socket,
            .status = "200 OK",
            .content_type = "application/json",
            .body = NULL,
            .set_cookie = {0},
            .keep_alive = keep_alive // Set the keep-alive status
        };

        // Call the handler for this route
        routes[i].handler(&req, &res);

        // Free allocated memory
        if (body)
        {
            free(body);
            body = NULL;
        }

        free_req(&parsed_query);
        free_req(&headers);
        free_req(&params);

        // Return whether to close the connection
        return res.keep_alive ? 0 : 1;
    }

    // If no route matches, return 404
    if (!route_found)
    {
        printf("No matching route found for: %s %s\n", method, path);

        Res res = {
            .client_socket = client_socket,
            .status = "404 Not Found",
            .content_type = "text/plain",
            .body = NULL,
            .set_cookie = {0},
            .keep_alive = keep_alive // Set the keep-alive status
        };

        reply(&res, res.status, res.content_type, "404 Not Found");

        // Return whether to close the connection
        return res.keep_alive ? 0 : 1;
    }

    // Free allocated resources
    free_req(&parsed_query);
    free_req(&headers);

    if (body)
    {
        free(body);
        body = NULL;
    }

    // Default to closing the connection
    return 1;
}

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

// Fix for reply function
void reply(Res *res, const char *status, const char *content_type, const char *body)
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
                           "HTTP/1.1 %s\r\n"         // Status code and status text
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
    printf("Sending response: %d bytes, status: %s\n", written, status);

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

// Fix for 400 Bad Request handling in router function
// Replace the relevant section in router() with this code:
void send_400_response(uv_tcp_t *client_socket)
{
    const char *bad_request = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: 11\r\nConnection: close\r\n\r\nBad Request";

    // Create a write request structure
    write_req_t *write_req = (write_req_t *)malloc(sizeof(write_req_t));
    if (!write_req)
    {
        fprintf(stderr, "Failed to allocate memory for write request\n");
        return;
    }

    // Allocate memory for the response and copy the bad request message
    char *response = malloc(strlen(bad_request) + 1);
    if (!response)
    {
        fprintf(stderr, "Failed to allocate memory for response\n");
        free(write_req);
        return;
    }

    strcpy(response, bad_request);

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
