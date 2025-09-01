#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include "uv.h"
#include "llhttp.h"
#include "../utils/cors.h"
#include "route_trie.h"
#include "middleware.h"

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
            free(write_req->data); // Using malloc/free for libuv-managed data
            write_req->data = NULL;
        }

        memset(write_req, 0, sizeof(write_req_t));
        free(write_req);
    }
}

// Sends error responses (400 or 500) - uses malloc for libuv
static void send_error(uv_tcp_t *client_socket, int error_code)
{
    if (!client_socket)
        return;

    if (uv_is_closing((uv_handle_t *)client_socket))
        return;

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

// Separates URL into path and query string components
// Example: /users/123?active=true -> path="/users/123", query="active=true"
static int extract_path_and_query(Arena *arena, char *url_buf, char **path, char **query)
{
    if (!arena || !url_buf || !path || !query)
        return -1;

    char *qmark = strchr(url_buf, '?');
    if (qmark)
    {
        *qmark = '\0';
        *path = arena_strdup(arena, url_buf);
        *query = arena_strdup(arena, qmark + 1);
    }
    else
    {
        *path = arena_strdup(arena, url_buf);
        *query = arena_strdup(arena, "");
    }

    if (!*path || !*query)
        return -1;

    // If path is empty, treat it as root
    if ((*path)[0] == '\0')
    {
        *path = arena_strdup(arena, "/");
        if (!*path)
            return -1;
    }
    return 0;
}

// Extracts URL parameters from a previously matched route
// Example: From route /users/:id matched with /users/123, extracts parameter id=123
static int extract_url_params(Arena *arena, const route_match_t *match, request_t *url_params)
{
    if (!arena || !match || !url_params)
        return -1;

    if (url_params->capacity == 0)
    {
        url_params->capacity = match->param_count > 0 ? match->param_count : 1;
        url_params->items = arena_alloc(arena, sizeof(request_item_t) * url_params->capacity);
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
        char *key = arena_alloc(arena, match->params[i].key.len + 1);
        char *value = arena_alloc(arena, match->params[i].value.len + 1);

        if (!key || !value)
        {
            return -1;
        }

        arena_memcpy(key, match->params[i].key.data, match->params[i].key.len);
        key[match->params[i].key.len] = '\0';

        arena_memcpy(value, match->params[i].value.data, match->params[i].value.len);
        value[match->params[i].value.len] = '\0';

        url_params->items[url_params->count].key = key;
        url_params->items[url_params->count].value = value;
        url_params->count++;
    }

    return 0;
}

// Context clearing
static void req_clear_context(Req *req)
{
    if (!req)
        return;

    req->context.data = NULL;
    req->context.size = 0;
    req->context.arena = NULL;
}

// Context management functions
void set_context(Req *req, void *data, size_t size)
{
    if (!req)
        return;

    // Clear existing context first
    req_clear_context(req);

    req->context.data = data;
    req->context.size = size;
    req->context.arena = req->arena;
}

void *get_context(Req *req)
{
    if (!req)
        return NULL;
    return req->context.data;
}

// Create and initialize Req
static Req *create_req(Arena *arena, uv_tcp_t *client_socket)
{
    if (!arena)
        return NULL;

    Req *req = arena_alloc(arena, sizeof(Req));
    if (!req)
        return NULL;

    memset(req, 0, sizeof(Req));
    req->arena = arena;
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
    req->context.arena = arena;

    return req;
}

// Create and initialize Res
static Res *create_res(Arena *arena, uv_tcp_t *client_socket)
{
    if (!arena)
        return NULL;

    Res *res = arena_alloc(arena, sizeof(Res));
    if (!res)
        return NULL;

    memset(res, 0, sizeof(Res));
    res->arena = arena;
    res->client_socket = client_socket;
    res->status = 200;
    res->content_type = arena_strdup(arena, "text/plain");
    res->body = NULL;
    res->body_len = 0;
    res->keep_alive = 1;
    res->headers = NULL;
    res->header_count = 0;
    res->header_capacity = 0;

    return res;
}

// Create and initialize http_context_t
static http_context_t *create_http_context(Arena *arena)
{
    if (!arena)
        return NULL;

    http_context_t *context = arena_alloc(arena, sizeof(http_context_t));
    if (!context)
        return NULL;

    http_context_init(context, arena);
    return context;
}

static request_t copy_request_t(Arena *arena, const request_t *original)
{
    request_t copy;
    memset(&copy, 0, sizeof(request_t));

    if (!original || original->count == 0)
        return copy;

    if (arena)
    {
        // Allocate items array in arena
        copy.capacity = original->capacity;
        copy.count = original->count;
        copy.items = arena_alloc(arena, copy.capacity * sizeof(request_item_t));

        if (!copy.items)
        {
            copy.capacity = 0;
            copy.count = 0;
            return copy;
        }

        // Copy each item using arena
        for (int i = 0; i < original->count; i++)
        {
            if (original->items[i].key)
            {
                copy.items[i].key = arena_strdup(arena, original->items[i].key);
                if (!copy.items[i].key)
                {
                    // Arena allocation failed - clear and return
                    memset(&copy, 0, sizeof(request_t));
                    return copy;
                }
            }
            else
            {
                copy.items[i].key = NULL;
            }

            if (original->items[i].value)
            {
                copy.items[i].value = arena_strdup(arena, original->items[i].value);
                if (!copy.items[i].value)
                {
                    // Arena allocation failed - clear and return
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
    else
    {
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
}

// Arena-aware request population
static int populate_req_from_context(Req *req, http_context_t *context, const char *path)
{
    if (!req || !req->arena || !context)
        return -1;

    Arena *arena = req->arena;

    // Copy method
    if (context->method)
    {
        req->method = arena_strdup(arena, context->method);
        if (!req->method)
            return -1;
    }

    // Copy path
    if (path)
    {
        req->path = arena_strdup(arena, path);
        if (!req->path)
            return -1;
    }

    // Copy body
    if (context->body && context->body_length > 0)
    {
        req->body = arena_alloc(arena, context->body_length + 1);
        if (!req->body)
            return -1;
        arena_memcpy(req->body, context->body, context->body_length);
        req->body[context->body_length] = '\0';
        req->body_len = context->body_length;
    }

    req->headers = copy_request_t(arena, &context->headers);
    req->query = copy_request_t(arena, &context->query_params);
    req->params = copy_request_t(arena, &context->url_params);

    return 0;
}

// Composes and sends the response (headers + body) using malloc for libuv data
void reply(Res *res, int status, const char *content_type, const void *body, size_t body_len)
{
    if (!res || !res->client_socket)
    {
        return;
    }

    if (uv_is_closing((uv_handle_t *)res->client_socket))
    {
        return;
    }

    if (!uv_is_readable((uv_stream_t *)res->client_socket) ||
        !uv_is_writable((uv_stream_t *)res->client_socket))
    {
        return;
    }

    if (!content_type)
        content_type = "text/plain";
    if (!body)
        body_len = 0;

    // Get current date in HTTP format
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    char date_str[64];
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", gmt);

    // Calculate total size of custom headers
    size_t headers_size = 0;
    for (int i = 0; i < res->header_count; i++)
    {
        if (res->headers[i].name && res->headers[i].value)
        {
            headers_size += strlen(res->headers[i].name) + 2 + strlen(res->headers[i].value) + 2;
        }
    }

    // Allocate and fill entire header string using malloc for libuv
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

    // Calculate response size
    int base_header_len = snprintf(
        NULL, 0,
        "HTTP/1.1 %d\r\n"
        "Server: Ecewo\r\n"
        "Date: %s\r\n"
        "%s"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "\r\n",
        status,
        date_str,
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

    // Use malloc for libuv-managed response
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
        "Server: Ecewo\r\n"
        "Date: %s\r\n"
        "%s"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "\r\n",
        status,
        date_str,
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
}

// Main router function
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

    // Create request arena on heap
    Arena *arena = calloc(1, sizeof(Arena));
    if (!arena)
    {
        send_error(client_socket, 500);
        return 1;
    }

    // Initialize all resources
    http_context_t *ctx = NULL;
    Req *req = NULL;
    Res *res = NULL;
    tokenized_path_t tokenized_path = {0};
    int error_code = 0;
    int should_close = 1;
    bool send_error_response = false;
    bool send_404_response = false;

    // Create resources using heap arena
    ctx = create_http_context(arena);
    req = create_req(arena, client_socket);
    res = create_res(arena, client_socket);

    if (!ctx || !req || !res)
    {
        error_code = 500;
        send_error_response = true;
        goto cleanup;
    }

    // Parse HTTP request
    enum llhttp_errno err = llhttp_execute(&ctx->parser, request_data, request_len);
    if (err != HPE_OK)
    {
        error_code = 400;
        send_error_response = true;
        goto cleanup;
    }

    // Extract path and query
    char *path = NULL;
    char *query = NULL;
    if (extract_path_and_query(arena, ctx->url, &path, &query) != 0)
    {
        error_code = 500;
        send_error_response = true;
        goto cleanup;
    }

    if (!path)
    {
        error_code = 400;
        send_error_response = true;
        goto cleanup;
    }

    // Parse query parameters
    parse_query(arena, query, &ctx->query_params);
    res->keep_alive = ctx->keep_alive;

    // Handle CORS preflight
    if (cors_handle_preflight(ctx, res))
    {
        reply(res, res->status, res->content_type, res->body, res->body_len);
        should_close = !res->keep_alive;
        goto cleanup;
    }

    // Route matching validation
    if (!global_route_trie || !ctx->method)
    {
        printf("ERROR: Missing route trie (%p) or method (%s)\n",
               global_route_trie, ctx->method ? ctx->method : "NULL");
        cors_add_headers(ctx, res);
        send_404_response = true;
        goto cleanup;
    }

    // Tokenize path
    if (tokenize_path(arena, path, &tokenized_path) != 0)
    {
        error_code = 500;
        send_error_response = true;
        goto cleanup;
    }

    // Route matching
    route_match_t match;
    if (!route_trie_match(global_route_trie, ctx->method, &tokenized_path, &match))
    {
        cors_add_headers(ctx, res);
        send_404_response = true;
        goto cleanup;
    }

    if (extract_url_params(arena, &match, &ctx->url_params) != 0)
    {
        error_code = 500;
        send_error_response = true;
        goto cleanup;
    }

    if (populate_req_from_context(req, ctx, path) != 0)
    {
        error_code = 500;
        send_error_response = true;
        goto cleanup;
    }

    if (!match.handler)
    {
        error_code = 500;
        send_error_response = true;
        goto cleanup;
    }

    // Success path - call handler
    cors_add_headers(ctx, res);

    // Calls chain if there is a middleware
    // otherwise, calls the handler directly
    if (match.middleware_ctx)
    {
        MiddlewareInfo *middleware_info = (MiddlewareInfo *)match.middleware_ctx;
        execute_middleware_chain(req, res, middleware_info);
    }
    else
    {
        // No middleware, call handler directly
        match.handler(req, res);
    }

    should_close = !res->keep_alive;

    goto cleanup;

cleanup:
    // Send error responses if needed
    if (send_error_response && error_code > 0)
    {
        send_error(client_socket, error_code);
    }
    else if (send_404_response)
    {
        const char *not_found_msg = "404 Not Found";
        reply(res, 404, "text/plain", not_found_msg, strlen(not_found_msg));
        should_close = !res->keep_alive;
    }

    return should_close;
}

// Adds a header
void set_header(Res *res, const char *name, const char *value)
{
    if (!res || !res->arena || !name || !value)
    {
        fprintf(stderr, "Error: Invalid argument(s) to set_header\n");
        return;
    }

    if (res->header_count >= res->header_capacity)
    {
        int new_cap = res->header_capacity ? res->header_capacity * 2 : 8;
        http_header_t *tmp;

        // Arena-based allocation
        tmp = arena_realloc(res->arena, res->headers,
                            res->header_capacity * sizeof(http_header_t),
                            new_cap * sizeof(http_header_t));

        if (!tmp)
        {
            fprintf(stderr, "Error: Failed to realloc headers array\n");
            return;
        }

        res->headers = tmp;
        res->header_capacity = new_cap;
    }

    // Arena-based string allocation
    res->headers[res->header_count].name = arena_strdup(res->arena, name);
    res->headers[res->header_count].value = arena_strdup(res->arena, value);

    if (!res->headers[res->header_count].name || !res->headers[res->header_count].value)
    {
        fprintf(stderr, "Error: Failed to allocate memory for header strings\n");
        return;
    }

    res->header_count++;
}
