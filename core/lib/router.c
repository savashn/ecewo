#include <stdlib.h>
#include <string.h>
#include "ecewo.h"
#include "uv.h"
#include "llhttp.h"
#include "cors.h"

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

// Initialize response structure
static void res_init(Res *res, uv_tcp_t *client_socket)
{
    res->client_socket = client_socket;
    res->status = 200;
    res->content_type = "text/plain";
    res->body = NULL;
    res->body_len = 0;
    res->set_cookie = NULL;
    res->keep_alive = 1;
    res->headers = NULL;
    res->header_count = 0;
    res->header_capacity = 0;
}

// Free response structure
static void res_free(Res *res)
{
    if (res->set_cookie)
    {
        free(res->set_cookie);
        res->set_cookie = NULL;
    }

    if (res->headers)
    {
        for (int i = 0; i < res->header_count; i++)
        {
            free(res->headers[i].name);
            free(res->headers[i].value);
        }
        free(res->headers);
        res->headers = NULL;
    }

    res->header_count = 0;
    res->header_capacity = 0;
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
        free(path);
        free(query);
        return 1; // Close connection for memory errors
    }

    // Debug info
    printf("Request Method: %s\n", context.method);
    printf("Request Path: %s\n", path);
    // printf("Request Query: %s\n", query);

    // Parse query parameters
    parse_query(query, &context.query_params);

    // Initialize response
    Res res;
    res_init(&res, client_socket);
    res.keep_alive = context.keep_alive;

    // Handle CORS preflight
    if (cors_handle_preflight(&context, &res))
    {
        reply(&res, res.status, res.content_type, res.body, res.body_len);

        // Cleanup
        res_free(&res);
        http_context_free(&context);
        free(path);
        free(query);

        return res.keep_alive ? 0 : 1;
    }

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

        // Detect async vs sync by handler signature: decide by returning value or flag
        // Here assume handlers that call async_execute manage their own free

        // Prepare request object
        Req req = {
            .client_socket = client_socket,
            .method = context.method,
            .path = path,
            .body = context.body,
            .body_len = context.body_length,
            .params = context.url_params,
            .query = context.query_params,
            .headers = context.headers,
        };

        // // Prepare response object with defaults
        // Res res = {
        //     .client_socket = client_socket,
        //     .status = 200,
        //     .content_type = "text/plain",
        //     .body = NULL,
        //     .set_cookie = NULL,
        //     .keep_alive = context.keep_alive // Set the keep-alive status
        // };

        // Add CORS headers if enabled
        cors_add_headers(&context, &res);

        // Call the handler for this route
        routes[i].handler(&req, &res);

        // Send response
        // reply(&res, res.status, res.content_type, res.body, res.body_len);

        // Cleanup
        res_free(&res);
        http_context_free(&context);
        free(path);
        free(query);

        return res.keep_alive ? 0 : 1;
    }

    // If no route matches, return 404
    if (!route_found)
    {
        printf("No matching route found for: %s %s\n", context.method, path);

        res.status = 404;
        res.content_type = "text/plain";

        // Add CORS headers for 404 responses too
        cors_add_headers(&context, &res);

        reply(&res, 404, "text/plain", "404 Not Found", SIZE_MAX);

        // Cleanup
        res_free(&res);
        http_context_free(&context);
        free(path);
        free(query);

        return res.keep_alive ? 0 : 1;
    }

    // Fallback cleanup
    res_free(&res);
    http_context_free(&context);
    free(path);
    free(query);

    return 1;
}

void reply(Res *res, int status, const char *content_type, const void *body, size_t body_len)
{
    // Determine payload length
    size_t payload_len;
    if (body_len == SIZE_MAX)
    {
        // If SIZE_MAX, treat as string and calculate length
        payload_len = body ? strlen((const char *)body) : 0;
    }
    else
    {
        // Use provided length (for binary data)
        payload_len = body_len;
    }

    // Build headers string
    size_t header_buffer_size = 1024; // Initial size
    char *all_headers = malloc(header_buffer_size);
    if (!all_headers)
    {
        perror("malloc for headers");
        return;
    }

    int header_pos = 0;

    // Add custom headers
    for (int i = 0; i < res->header_count; i++)
    {
        int needed = snprintf(NULL, 0, "%s: %s\r\n", res->headers[i].name, res->headers[i].value);
        if (header_pos + needed >= header_buffer_size)
        {
            header_buffer_size = (header_pos + needed + 1) * 2;
            char *new_buffer = realloc(all_headers, header_buffer_size);
            if (!new_buffer)
            {
                perror("realloc for headers");
                free(all_headers);
                return;
            }
            all_headers = new_buffer;
        }
        header_pos += sprintf(all_headers + header_pos, "%s: %s\r\n",
                              res->headers[i].name, res->headers[i].value);
    }

    // Add cookie header if present
    if (res->set_cookie)
    {
        int needed = snprintf(NULL, 0, "Set-Cookie: %s\r\n", res->set_cookie);
        if (header_pos + needed >= header_buffer_size)
        {
            header_buffer_size = (header_pos + needed + 1) * 2;
            char *new_buffer = realloc(all_headers, header_buffer_size);
            if (!new_buffer)
            {
                perror("realloc for headers");
                free(all_headers);
                return;
            }
            all_headers = new_buffer;
        }
        header_pos += sprintf(all_headers + header_pos, "Set-Cookie: %s\r\n", res->set_cookie);
    }

    all_headers[header_pos] = '\0';

    // Calculate total header length
    int base_header_len = snprintf(
        NULL, 0,
        "HTTP/1.1 %d\r\n"
        "%s"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "\r\n",
        status,
        all_headers,
        content_type,
        payload_len,
        res->keep_alive ? "keep-alive" : "close");

    if (base_header_len < 0)
    {
        fprintf(stderr, "Failed to compute header length\n");
        free(all_headers);
        return;
    }

    // Allocate response buffer
    size_t total_len = (size_t)base_header_len + payload_len;
    char *response = malloc(total_len);
    if (!response)
    {
        perror("malloc for response");
        free(all_headers);
        return;
    }

    // Write headers
    int written = snprintf(
        response,
        (size_t)base_header_len + 1,
        "HTTP/1.1 %d\r\n"
        "%s"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "\r\n",
        status,
        all_headers,
        content_type,
        payload_len,
        res->keep_alive ? "keep-alive" : "close");

    free(all_headers);

    if (written < 0)
    {
        fprintf(stderr, "Response formatting error\n");
        free(response);
        return;
    }

    // Copy body (binary safe)
    if (payload_len > 0 && body)
    {
        memcpy(response + written, body, payload_len);
    }

    // Debug info
    printf("Sending response: %zu bytes, status: %d\n", total_len, status);

    // Create write request
    write_req_t *write_req = malloc(sizeof(write_req_t));
    if (!write_req)
    {
        fprintf(stderr, "Failed to allocate memory for write request\n");
        free(response);
        return;
    }

    // Store the allocated buffer in the write request so we can free it later
    write_req->data = response;

    // Set up the buffer for libuv
    write_req->buf = uv_buf_init(response, (unsigned int)total_len);

    // Send response
    int result = uv_write(&write_req->req, (uv_stream_t *)res->client_socket,
                          &write_req->buf, 1, write_completion_cb);
    if (result < 0)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(result));
        free(response);
        free(write_req);
    }
}

// Add response header
void set_header(Res *res, const char *name, const char *value)
{
    if (!name || !value)
        return;

    // Check if we need to expand the headers array
    if (res->header_count >= res->header_capacity)
    {
        int new_capacity = res->header_capacity == 0 ? 8 : res->header_capacity * 2;
        http_header_t *new_headers = realloc(res->headers, new_capacity * sizeof(http_header_t));
        if (!new_headers)
        {
            fprintf(stderr, "Failed to allocate memory for headers\n");
            return;
        }
        res->headers = new_headers;
        res->header_capacity = new_capacity;
    }

    // Add the header
    res->headers[res->header_count].name = strdup(name);
    res->headers[res->header_count].value = strdup(value);

    if (!res->headers[res->header_count].name || !res->headers[res->header_count].value)
    {
        fprintf(stderr, "Failed to allocate memory for header strings\n");
        if (res->headers[res->header_count].name)
            free(res->headers[res->header_count].name);
        if (res->headers[res->header_count].value)
            free(res->headers[res->header_count].value);
        return;
    }

    res->header_count++;
}
