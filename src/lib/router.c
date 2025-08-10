#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "ecewo.h"
#include "uv.h"
#include "llhttp.h"
#include "cors.h"
#include "route_trie.h"

// Called when write operation is completed
static void write_completion_cb(uv_write_t *req, int status)
{
    if (status < 0)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(status));
    }

    write_req_t *write_req = (write_req_t *)req;
    if (write_req)
    {
        if (write_req->data)
        {
            free(write_req->data);
            write_req->data = NULL;
        }

        memset(write_req, 0, sizeof(write_req_t));
        free(write_req);
    }
}

// Sends error responses (400 or 500)
static void send_error(uv_tcp_t *client_socket, int error_code)
{
    if (!client_socket)
        return;

    // Handle status check
    if (uv_is_closing((uv_handle_t *)client_socket))
        return;

    // Stream status check
    if (!uv_is_readable((uv_stream_t *)client_socket) ||
        !uv_is_writable((uv_stream_t *)client_socket))
        return;

    const char *err = NULL;
    const char *err_500 = "HTTP/1.1 500 Internal Server Error\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: 21\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "Internal Server Error";

    const char *err_400 = "HTTP/1.1 400 Bad Request\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: 11\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "Bad Request";

    if (error_code == 500)
        err = err_500;
    else if (error_code == 400)
        err = err_400;
    else
        return;

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

    memcpy(response, err, len);
    response[len] = '\0';

    memset(write_req, 0, sizeof(write_req_t));
    write_req->data = response;
    write_req->buf = uv_buf_init(response, (unsigned int)len);

    if (uv_is_closing((uv_handle_t *)client_socket))
    {
        free(response);
        free(write_req);
        return;
    }

    int res = uv_write(&write_req->req, (uv_stream_t *)client_socket,
                       &write_req->buf, 1, write_completion_cb);
    if (res < 0)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(res));
        free(response);
        free(write_req);
    }
}

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

static int extract_url_params(const route_match_t *match, request_t *url_params)
{
    if (!match || !url_params)
        return -1;

    if (url_params->capacity == 0)
    {
        url_params->capacity = match->param_count > 0 ? match->param_count : 1;
        url_params->items = malloc(sizeof(request_item_t) * url_params->capacity);
        if (!url_params->items)
        {
            url_params->capacity = 0;
            return -1;
        }

        for (int i = 0; i < url_params->capacity; i++)
        {
            url_params->items[i].key = NULL;
            url_params->items[i].value = NULL;
        }
    }

    for (int i = 0; i < match->param_count && url_params->count < url_params->capacity; i++)
    {
        char *key = malloc(match->params[i].key.len + 1);
        char *value = malloc(match->params[i].value.len + 1);

        if (!key || !value)
        {
            free(key);
            free(value);
            return -1;
        }

        memcpy(key, match->params[i].key.data, match->params[i].key.len);
        key[match->params[i].key.len] = '\0';

        memcpy(value, match->params[i].value.data, match->params[i].value.len);
        value[match->params[i].value.len] = '\0';

        url_params->items[url_params->count].key = key;
        url_params->items[url_params->count].value = value;
        url_params->count++;
    }

    return 0;
}

// Run at the begining of set_context and destroy_req
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

// Create and initialize Req on heap
static Req *create_req(uv_tcp_t *client_socket)
{
    Req *req = malloc(sizeof(Req));
    if (!req)
        return NULL;

    memset(req, 0, sizeof(Req));
    req->client_socket = client_socket;
    req->method = NULL;
    req->path = NULL;
    req->body = NULL;
    req->body_len = 0;

    // Initialize request_t structures
    memset(&req->headers, 0, sizeof(request_t));
    memset(&req->query, 0, sizeof(request_t));
    memset(&req->params, 0, sizeof(request_t));

    // Initialize context
    req->context.data = NULL;
    req->context.size = 0;
    req->context.cleanup = NULL;

    return req;
}

// Create and initialize Res on heap
static Res *create_res(uv_tcp_t *client_socket)
{
    Res *res = malloc(sizeof(Res));
    if (!res)
        return NULL;

    memset(res, 0, sizeof(Res));
    res->client_socket = client_socket;
    res->status = 200;
    res->content_type = "text/plain";
    res->body = NULL;
    res->body_len = 0;
    res->keep_alive = 1;
    res->headers = NULL;
    res->header_count = 0;
    res->header_capacity = 0;

    return res;
}

// Create and initialize http_context_t on heap
static http_context_t *create_http_context(void)
{
    http_context_t *context = malloc(sizeof(http_context_t));
    if (!context)
        return NULL;

    http_context_init(context);
    return context;
}

// Destroy http_context_t
static void destroy_http_context(http_context_t *context)
{
    if (!context)
        return;

    http_context_free(context);
    free(context);
}

// Cleanup function for request_t
static void cleanup_request_t(request_t *req_data)
{
    if (!req_data || !req_data->items)
        return;

    for (int i = 0; i < req_data->count; i++)
    {
        free(req_data->items[i].key);
        free(req_data->items[i].value);
    }

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

// Destroy Req
void destroy_req(Req *req)
{
    if (!req)
        return;

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

// Destroy Res
void destroy_res(Res *res)
{
    if (!res)
        return;

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

    free(res);
}

static int populate_req_from_context(Req *req, http_context_t *context, const char *path)
{
    // Copy method
    if (context->method)
    {
        req->method = strdup(context->method);
        if (!req->method)
            return -1;
    }

    // Copy path
    if (path)
    {
        req->path = strdup(path);
        if (!req->path)
            return -1;
    }

    // Copy body
    if (context->body && context->body_length > 0)
    {
        req->body = malloc(context->body_length + 1);
        if (!req->body)
            return -1;
        memcpy(req->body, context->body, context->body_length);
        req->body[context->body_length] = '\0';
        req->body_len = context->body_length;
    }

    req->headers = copy_request_t(&context->headers);
    req->query = copy_request_t(&context->query_params);
    req->params = copy_request_t(&context->url_params);

    return 0;
}

// Composes and sends the response (headers + body)
void reply(Res *res, int status, const char *content_type, const void *body, size_t body_len)
{
    // Comprehensive validation
    if (!res || !res->client_socket)
    {
        return;
    }

    // Check if handle is valid and not closing
    if (uv_is_closing((uv_handle_t *)res->client_socket))
    {
        return;
    }

    // Check if the handle is still readable/writable
    if (!uv_is_readable((uv_stream_t *)res->client_socket) ||
        !uv_is_writable((uv_stream_t *)res->client_socket))
    {
        return;
    }

    // Validate parameters
    if (!content_type)
        content_type = "text/plain";
    if (!body)
        body_len = 0;

    // Calculate total size of custom headers
    size_t headers_size = 0;
    for (int i = 0; i < res->header_count; i++)
    {
        if (res->headers[i].name && res->headers[i].value)
        {
            headers_size += strlen(res->headers[i].name) + 2 + strlen(res->headers[i].value) + 2;
        }
    }

    // Allocate and fill entire header string
    char *all_headers = malloc(headers_size + 1);
    if (!all_headers)
    {
        send_error(res->client_socket, 500);
        return;
    }

    size_t pos = 0;
    for (int i = 0; i < res->header_count; i++)
    {
        if (res->headers[i].name && res->headers[i].value)
        {
            int n = snprintf(all_headers + pos, headers_size - pos + 1,
                             "%s: %s\r\n", res->headers[i].name, res->headers[i].value);
            if (n > 0 && (size_t)n <= headers_size - pos)
            {
                pos += n;
            }
            else
            {
                free(all_headers);
                send_error(res->client_socket, 500);
                return;
            }
        }
    }
    all_headers[pos] = '\0';

    // Calculate response size more safely
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
        send_error(res->client_socket, 500);
        return;
    }

    size_t total_len = (size_t)base_header_len + body_len;

    char *response = malloc(total_len + 1);
    if (!response)
    {
        free(all_headers);
        send_error(res->client_socket, 500);
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

    if (written < 0 || (size_t)written > total_len)
    {
        free(response);
        send_error(res->client_socket, 500);
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
        send_error(res->client_socket, 500);
        return;
    }

    memset(write_req, 0, sizeof(write_req_t));
    write_req->data = response;
    write_req->buf = uv_buf_init(response, (unsigned int)total_len);

    // Final check before writing
    if (uv_is_closing((uv_handle_t *)res->client_socket))
    {
        free(response);
        free(write_req);
        return;
    }

    int result = uv_write(&write_req->req, (uv_stream_t *)res->client_socket,
                          &write_req->buf, 1, write_completion_cb);
    if (result < 0)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(result));
        free(response);
        free(write_req);
        return;
    }

    // Success
    // The write_completion_cb will do the cleanup
}

// Called when a request is received
int router(uv_tcp_t *client_socket, const char *request_data, size_t request_len)
{
    // Early validation
    if (!client_socket || !request_data || request_len == 0)
    {
        if (client_socket)
            send_error(client_socket, 400);
        return 1;
    }

    if (uv_is_closing((uv_handle_t *)client_socket))
        return 1;

    http_context_t *ctx = create_http_context();
    Req *req = create_req(client_socket);
    Res *res = create_res(client_socket);

    if (!ctx || !req || !res)
    {
        destroy_http_context(ctx);
        destroy_req(req);
        destroy_res(res);
        send_error(client_socket, 500);
        return 1;
    }

    // Parse HTTP request
    enum llhttp_errno err = llhttp_execute(&ctx->parser, request_data, request_len);
    if (err != HPE_OK)
    {
        fprintf(stderr, "HTTP parsing error: %s\n", llhttp_errno_name(err));
        send_error(client_socket, 400);
        destroy_http_context(ctx);
        destroy_req(req);
        destroy_res(res);
        return 1;
    }

    // Extract path and query
    char *path = NULL;
    char *query = NULL;
    if (extract_path_and_query(ctx->url, &path, &query) != 0)
    {
        send_error(client_socket, 500);
        destroy_http_context(ctx);
        destroy_req(req);
        destroy_res(res);
        return 1;
    }

    if (!path)
    {
        send_error(client_socket, 400);
        destroy_http_context(ctx);
        destroy_req(req);
        destroy_res(res);
        return 1;
    }

    // Parse query parameters
    parse_query(query, &ctx->query_params);

    // Set keep-alive from context
    res->keep_alive = ctx->keep_alive;

    // Handle CORS preflight
    if (cors_handle_preflight(ctx, res))
    {
        reply(res, res->status, res->content_type, res->body, res->body_len);
        int should_close = !res->keep_alive;
        destroy_http_context(ctx);
        destroy_req(req);
        destroy_res(res);
        return should_close;
    }

    // Route matching
    if (!global_route_trie || !ctx->method)
    {
        cors_add_headers(ctx, res);
        const char *not_found_msg = "404 Not Found";
        reply(res, 404, "text/plain", not_found_msg, strlen(not_found_msg));
        int should_close = !res->keep_alive;
        destroy_http_context(ctx);
        destroy_req(req);
        destroy_res(res);
        return should_close;
    }

    route_match_t match;
    if (route_trie_match(global_route_trie, ctx->method, path, &match))
    {
        if (match.param_count > 0)
        {
            if (extract_url_params(&match, &ctx->url_params) != 0)
            {
                send_error(client_socket, 500);
                destroy_http_context(ctx);
                destroy_req(req);
                destroy_res(res);
                return 1;
            }
        }

        // Populate request from context
        if (populate_req_from_context(req, ctx, path) != 0)
        {
            send_error(client_socket, 500);
            destroy_http_context(ctx);
            destroy_req(req);
            destroy_res(res);
            return 1;
        }

        if (!match.handler)
        {
            send_error(client_socket, 500);
            destroy_http_context(ctx);
            destroy_req(req);
            destroy_res(res);
            return 1;
        }

        // Execute handler (middleware-wrapped)
        cors_add_headers(ctx, res);
        match.handler(req, res);

        int should_close = !res->keep_alive;
        destroy_http_context(ctx);
        destroy_req(req);
        destroy_res(res);
        return should_close;
    }

    // 404 Not Found
    cors_add_headers(ctx, res);
    const char *not_found_msg = "404 Not Found";
    reply(res, 404, "text/plain", not_found_msg, strlen(not_found_msg));
    int should_close = !res->keep_alive;
    destroy_http_context(ctx);
    destroy_req(req);
    destroy_res(res);
    return should_close;
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
    memset(&copy->context, 0, sizeof(copy->context));
    copy->context.data = NULL;
    copy->context.size = 0;
    copy->context.cleanup = NULL;

    return copy;
}
