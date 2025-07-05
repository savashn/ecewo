#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ecewo.h"
#include "uv.h"
#include "llhttp.h"
#include "cors.h"

// Called when write operation is completed
void write_completion_cb(uv_write_t *req, int status)
{
    if (status < 0)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(status));
    }
    write_req_t *write_req = (write_req_t *)req;
    free(write_req->data);
    free(write_req);
}

// Sends error responses (400 or 500)
static void send_error(uv_tcp_t *client_socket, int error_code)
{
    const char *err = NULL;
    if (error_code == 500)
    {
        err = "HTTP/1.1 500 Internal Server Error\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: 21\r\n"
              "Connection: close\r\n"
              "\r\n"
              "Internal Server Error";
    }
    else if (error_code == 400)
    {
        err = "HTTP/1.1 400 Bad Request\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: 11\r\n"
              "Connection: close\r\n"
              "\r\n"
              "Bad Request";
    }
    else
    {
        return;
    }

    write_req_t *write_req = malloc(sizeof(write_req_t));
    if (!write_req)
        return;

    size_t len = strlen(err);
    char *response = malloc(len + 1);
    if (!response)
    {
        free(write_req);
        return;
    }
    memcpy(response, err, len + 1);
    write_req->data = response;
    write_req->buf = uv_buf_init(response, (unsigned int)len);

    int res = uv_write(&write_req->req, (uv_stream_t *)client_socket, &write_req->buf, 1, write_completion_cb);
    if (res < 0)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(res));
        free(response);
        free(write_req);
    }
}

// matcher: matches a path with a dynamic parameter route using pointers, no copy
// Segments starting with ":param" in route_path are treated as parameters
bool matcher(const char *path, const char *route_path)
{
    if (!path || !route_path)
        return false;

    const char *p = path;
    const char *r = route_path;

    while (*p || *r)
    {
        if (*p == '\0' && *r == '\0')
            return true;

        if (*p == '\0' || *r == '\0')
            return false;

        // Segment beginning: '/'
        if (*r == '/')
        {
            if (*p != '/')
                return false;
            r++;
            p++;
            continue;
        }

        // Segment comparison
        // If the route segment starts with ':', skip the segment in the path
        if (*r == ':')
        {
            // Skip to end of segment in route
            while (*r && *r != '/')
                r++;
            // Skip to end of segment in path
            while (*p && *p != '/')
                p++;
        }
        else
        {
            // Static segment: compare character by character
            if (*p != *r)
                return false;
            p++;
            r++;
        }

        // Loop continues at end of each segment ('/' or end of string)
    }

    // Match if both reached end
    return (*p == '\0' && *r == '\0');
}

// extract_path_and_query: malloc-free. Replaces '?' with '\0' in context.url
// and returns pointers to path and query
static int extract_path_and_query(char *url_buf, char **path, char **query)
{
    if (!url_buf || !path || !query)
        return -1;

    // URL buffer already null-terminated by llhttp parser.
    // Example: "/users/1234?active=true"

    char *qmark = strchr(url_buf, '?');
    if (qmark)
    {
        *qmark = '\0';
        *path = url_buf;
        *query = qmark + 1;
    }
    else
    {
        *path = url_buf;
        *query = "";
    }

    // If path is empty, treat it as root
    if ((*path)[0] == '\0')
    {
        *path = "/";
    }
    return 0;
}

// Initializes the Res struct
static void res_init(Res *res, uv_tcp_t *client_socket)
{
    res->client_socket = client_socket;
    res->status = 200;
    res->content_type = "text/plain";
    res->body = NULL;
    res->body_len = 0;
    res->keep_alive = 1;
    res->headers = NULL;
    res->header_count = 0;
    res->header_capacity = 0;
}

// Frees the headers
static void res_clean(Res *res)
{
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

// Initializes the Req context
static void req_init_context(Req *req)
{
    req->context.data = NULL;
    req->context.size = 0;
    req->context.cleanup = NULL;
}

static void req_clear_context(Req *req)
{
    if (!req)
        return;

    if (req->context.data && req->context.cleanup)
    {
        req->context.cleanup(req->context.data);
    }

    req->context.data = NULL;
    req->context.size = 0;
    req->context.cleanup = NULL;
}

// Context management functions
void set_context(Req *req, void *data, size_t size, void (*cleanup)(void *))
{
    if (!req)
        return;

    // Clear existing context first
    req_clear_context(req);

    req->context.data = data;
    req->context.size = size;
    req->context.cleanup = cleanup;
}

void *get_context(Req *req)
{
    if (!req)
        return NULL;
    return req->context.data;
}

// Called when a request is received
int router(uv_tcp_t *client_socket, const char *request_data, size_t request_len)
{
    if (!request_data || request_len == 0)
    {
        send_error(client_socket, 400);
        return 1;
    }

    http_context_t context;
    http_context_init(&context);

    enum llhttp_errno err = llhttp_execute(&context.parser, request_data, request_len);
    if (err != HPE_OK)
    {
        fprintf(stderr, "HTTP parsing error: %s\n", llhttp_errno_name(err));
        send_error(client_socket, 400);
        http_context_free(&context);
        return 1;
    }

    // Parser already holds context.url in a null-terminated buffer, so we can
    // overwrite it to get path and query
    char *path = NULL;
    char *query = NULL;
    if (extract_path_and_query(context.url, &path, &query) != 0)
    {
        send_error(client_socket, 500);
        http_context_free(&context);
        return 1;
    }

    parse_query(query, &context.query_params);

    Res res;
    res_init(&res, client_socket);
    res.keep_alive = context.keep_alive;

    // CORS preflight
    if (cors_handle_preflight(&context, &res))
    {
        reply(&res, res.status, res.content_type, res.body, res.body_len);
        res_clean(&res);
        http_context_free(&context);
        return res.keep_alive ? 0 : 1;
    }

    bool route_found = false;
    for (int i = 0; i < route_count; i++)
    {
        if (strcasecmp(context.method, routes[i].method) != 0)
            continue;
        if (!matcher(path, routes[i].path))
            continue;

        route_found = true;
        parse_params(path, routes[i].path, &context.url_params);

        Req req = {
            .client_socket = client_socket,
            .method = context.method,
            .path = path,
            .body = context.body,
            .body_len = context.body_length,
            .params = context.url_params,
            .query = context.query_params,
            .headers = context.headers};

        // Initialize context
        req_init_context(&req);

        cors_add_headers(&context, &res);
        routes[i].handler(&req, &res);

        // Cleanup context after handler execution
        req_clear_context(&req);

        res_clean(&res);
        http_context_free(&context);
        return res.keep_alive ? 0 : 1;
    }

    if (!route_found)
    {
        res.status = 404;
        res.content_type = "text/plain";
        cors_add_headers(&context, &res);
        const char *not_found_msg = "404 Not Found";
        reply(&res, 404, "text/plain", not_found_msg, strlen(not_found_msg));
        res_clean(&res);
        http_context_free(&context);
        return res.keep_alive ? 0 : 1;
    }

    // Should never reach here; either route matched or 404 sent
    return 1;
}

// Composes and sends the response (headers + body)
void reply(Res *res, int status, const char *content_type, const void *body, size_t body_len)
{
    // Calculate total size of custom headers
    size_t headers_size = 0;
    for (int i = 0; i < res->header_count; i++)
    {
        // "Name: Value\r\n"
        headers_size += strlen(res->headers[i].name) + 2 + strlen(res->headers[i].value) + 2;
    }

    // Allocate and fill entire header string
    char *all_headers = malloc(headers_size + 1);
    if (!all_headers)
        return;
    size_t pos = 0;
    for (int i = 0; i < res->header_count; i++)
    {
        int n = sprintf(all_headers + pos, "%s: %s\r\n", res->headers[i].name, res->headers[i].value);
        pos += n;
    }
    all_headers[pos] = '\0';

    // Now calculate size of entire header + custom headers + body
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
        body_len,
        res->keep_alive ? "keep-alive" : "close");
    if (base_header_len < 0)
    {
        free(all_headers);
        return;
    }

    size_t total_len = (size_t)base_header_len + body_len;
    char *response = malloc(total_len);
    if (!response)
    {
        free(all_headers);
        return;
    }

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
        body_len,
        res->keep_alive ? "keep-alive" : "close");
    free(all_headers);
    if (written < 0)
    {
        free(response);
        return;
    }

    if (body_len > 0 && body)
    {
        memcpy(response + written, body, body_len);
    }

    write_req_t *write_req = malloc(sizeof(write_req_t));
    if (!write_req)
    {
        free(response);
        return;
    }
    write_req->data = response;
    write_req->buf = uv_buf_init(response, (unsigned int)total_len);

    int result = uv_write(&write_req->req, (uv_stream_t *)res->client_socket, &write_req->buf, 1, write_completion_cb);
    if (result < 0)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(result));
        free(response);
        free(write_req);
    }
}

// Adds a header
void set_header(Res *res, const char *name, const char *value)
{
    if (!name || !value)
        return;
    if (res->header_count >= res->header_capacity)
    {
        int new_cap = res->header_capacity ? res->header_capacity * 2 : 8;
        http_header_t *tmp = realloc(res->headers, new_cap * sizeof(http_header_t));
        if (!tmp)
            return;
        res->headers = tmp;
        res->header_capacity = new_cap;
    }
    res->headers[res->header_count].name = strdup(name);
    res->headers[res->header_count].value = strdup(value);
    if (!res->headers[res->header_count].name || !res->headers[res->header_count].value)
    {
        // In case of failure, do not attempt to use the incomplete entry
        free(res->headers[res->header_count].name);
        free(res->headers[res->header_count].value);
        return;
    }
    res->header_count++;
}

// Deep copy function for Res
Res *copy_res(const Res *original)
{
    if (!original)
        return NULL;

    Res *copy = malloc(sizeof(Res));
    if (!copy)
        return NULL;

    // Copy primitive fields
    *copy = *original;
    copy->body = original->body; // pointer only, not deep copied
    copy->content_type = original->content_type;

    // Allocate and copy headers array
    if (original->header_capacity > 0)
    {
        size_t cap = original->header_capacity;
        copy->headers = malloc(cap * sizeof(http_header_t));
        if (!copy->headers)
        {
            free(copy);
            return NULL;
        }

        for (int i = 0; i < original->header_count; ++i)
        {
            // Duplicate name
            if (original->headers[i].name)
            {
                copy->headers[i].name = strdup(original->headers[i].name);
                if (!copy->headers[i].name)
                {
                    destroy_res(copy);
                    return NULL;
                }
            }
            else
            {
                copy->headers[i].name = NULL;
            }
            // Duplicate value
            if (original->headers[i].value)
            {
                copy->headers[i].value = strdup(original->headers[i].value);
                if (!copy->headers[i].value)
                {
                    destroy_res(copy);
                    return NULL;
                }
            }
            else
            {
                copy->headers[i].value = NULL;
            }
        }
    }
    else
    {
        copy->headers = NULL;
    }

    return copy;
}

// Cleanup function for
void destroy_res(Res *res)
{
    if (!res)
        return;

    res_clean(res);

    free(res);
}

// Cleanup function for request_t
static void cleanup_request_t(request_t *req_data)
{
    if (!req_data || !req_data->items)
        return;

    // Free all key-value pairs
    for (int i = 0; i < req_data->count; i++)
    {
        if (req_data->items[i].key)
        {
            free(req_data->items[i].key);
            req_data->items[i].key = NULL;
        }
        if (req_data->items[i].value)
        {
            free(req_data->items[i].value);
            req_data->items[i].value = NULL;
        }
    }

    // Free items array
    free(req_data->items);
    req_data->items = NULL;
    req_data->count = 0;
    req_data->capacity = 0;
}

// Deep copy function for request_t
static request_t copy_request_t(const request_t *original)
{
    request_t copy;
    memset(&copy, 0, sizeof(request_t));

    if (!original || original->count == 0)
    {
        return copy;
    }

    // Allocate items array
    copy.capacity = original->capacity;
    copy.count = original->count;
    copy.items = malloc(copy.capacity * sizeof(request_item_t));

    if (!copy.items)
    {
        // Allocation failed, return empty
        copy.capacity = 0;
        copy.count = 0;
        return copy;
    }

    // Copy each item
    for (int i = 0; i < original->count; i++)
    {
        // Copy key
        if (original->items[i].key)
        {
            copy.items[i].key = strdup(original->items[i].key);
            if (!copy.items[i].key)
            {
                // Cleanup on failure
                for (int j = 0; j < i; j++)
                {
                    free(copy.items[j].key);
                    free(copy.items[j].value);
                }
                free(copy.items);
                memset(&copy, 0, sizeof(request_t));
                return copy;
            }
        }
        else
        {
            copy.items[i].key = NULL;
        }

        // Copy value
        if (original->items[i].value)
        {
            copy.items[i].value = strdup(original->items[i].value);
            if (!copy.items[i].value)
            {
                // Cleanup on failure
                free(copy.items[i].key);
                for (int j = 0; j < i; j++)
                {
                    free(copy.items[j].key);
                    free(copy.items[j].value);
                }
                free(copy.items);
                memset(&copy, 0, sizeof(request_t));
                return copy;
            }
        }
        else
        {
            copy.items[i].value = NULL;
        }
    }

    return copy;
}

// Deep copy function for Req
Req *copy_req(const Req *original)
{
    if (!original)
        return NULL;

    Req *copy = malloc(sizeof(Req));
    if (!copy)
        return NULL;

    // Copy primitive fields
    copy->client_socket = original->client_socket;
    copy->body_len = original->body_len;

    // Deep copy method string
    if (original->method)
    {
        copy->method = strdup(original->method);
        if (!copy->method)
        {
            free(copy);
            return NULL;
        }
    }
    else
    {
        copy->method = NULL;
    }

    // Deep copy path string
    if (original->path)
    {
        copy->path = strdup(original->path);
        if (!copy->path)
        {
            if (copy->method)
                free((void *)copy->method);
            free(copy);
            return NULL;
        }
    }
    else
    {
        copy->path = NULL;
    }

    // Deep copy body
    if (original->body && original->body_len > 0)
    {
        copy->body = malloc(original->body_len + 1);
        if (!copy->body)
        {
            if (copy->method)
                free((void *)copy->method);
            if (copy->path)
                free((void *)copy->path);
            free(copy);
            return NULL;
        }
        memcpy(copy->body, original->body, original->body_len);
        copy->body[original->body_len] = '\0'; // Null terminate
    }
    else
    {
        copy->body = NULL;
    }

    // Deep copy headers, query, params
    // You'll need to implement copy_request_t based on your request_t structure
    copy->headers = copy_request_t(&original->headers);
    copy->query = copy_request_t(&original->query);
    copy->params = copy_request_t(&original->params);

    // Initialize context (don't copy original context, start fresh)
    req_init_context(copy);

    return copy;
}

// Cleanup function for copied Req
void destroy_req(Req *req)
{
    if (!req)
        return;

    // Clear context first
    req_clear_context(req);

    // Free allocated strings
    if (req->method)
    {
        free((void *)req->method);
        req->method = NULL;
    }

    if (req->path)
    {
        free((void *)req->path);
        req->path = NULL;
    }

    if (req->body)
    {
        free(req->body);
        req->body = NULL;
    }

    // Free request_t structures
    cleanup_request_t(&req->headers);
    cleanup_request_t(&req->query);
    cleanup_request_t(&req->params);

    free(req);
}
