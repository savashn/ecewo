#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include "uv.h"
#include "llhttp.h"
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
        if (write_req->arena)
        {
            Arena *arena = write_req->arena;
            arena_free(arena);
            free(arena);
        }
        else
        {
            // This is necessary for early errors
            // that don't have arena yet
            // and called send_error
            if (write_req->data)
            {
                free(write_req->data);
                write_req->data = NULL;
            }
            memset(write_req, 0, sizeof(write_req_t));
            free(write_req);
        }
    }
}

// Sends error responses (400 or 500)
static void send_error(Arena *arena, uv_tcp_t *client_socket, int error_code)
{
    if (!client_socket)
        return;

    if (uv_is_closing((uv_handle_t *)client_socket))
        return;

    if (!uv_is_readable((uv_stream_t *)client_socket) ||
        !uv_is_writable((uv_stream_t *)client_socket))
        return;

    // Generate current date
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    char date_str[64];
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", gmt);

    const char *status_text = (error_code == 500) ? "Internal Server Error" : "Bad Request";
    const char *body = status_text;
    size_t body_len = strlen(body);

    if (arena)
    {
        // Arena path
        char *response = arena_sprintf(arena,
                                       "HTTP/1.1 %d %s\r\n"
                                       "Date: %s\r\n"
                                       "Content-Type: text/plain\r\n"
                                       "Content-Length: %zu\r\n"
                                       "Connection: close\r\n"
                                       "\r\n"
                                       "%s",
                                       error_code,
                                       status_text,
                                       date_str,
                                       body_len,
                                       body);

        if (!response)
        {
            arena_free(arena);
            free(arena);
            return;
        }

        size_t response_len = strlen(response);

        write_req_t *write_req = arena_alloc(arena, sizeof(write_req_t));
        if (!write_req)
        {
            arena_free(arena);
            free(arena);
            return;
        }

        memset(&write_req->req, 0, sizeof(uv_write_t));
        write_req->data = response;
        write_req->arena = arena;
        write_req->buf = uv_buf_init(response, (unsigned int)response_len);

        int res = uv_write(&write_req->req, (uv_stream_t *)client_socket,
                           &write_req->buf, 1, write_completion_cb);
        if (res < 0)
        {
            fprintf(stderr, "Write error: %s\n", uv_strerror(res));
            arena_free(arena);
            free(arena);
        }
    }
    else
    {
        // Malloc path
        // This is necessary for early errors
        // that don't have arena yet
        // and called send_error
        size_t response_size = 512;
        char *response = malloc(response_size);

        if (!response)
            return;

        int written = snprintf(response, response_size,
                               "HTTP/1.1 %d %s\r\n"
                               "Date: %s\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: %zu\r\n"
                               "Connection: close\r\n"
                               "\r\n"
                               "%s",
                               error_code,
                               status_text,
                               date_str,
                               body_len,
                               body);

        if (written < 0 || (size_t)written >= response_size)
        {
            free(response);
            return;
        }

        write_req_t *write_req = malloc(sizeof(write_req_t));
        if (!write_req)
        {
            free(response);
            return;
        }

        memset(&write_req->req, 0, sizeof(uv_write_t));
        write_req->data = response;
        write_req->arena = NULL;
        write_req->buf = uv_buf_init(response, (unsigned int)written);

        int res = uv_write(&write_req->req, (uv_stream_t *)client_socket,
                           &write_req->buf, 1, write_completion_cb);
        if (res < 0)
        {
            fprintf(stderr, "Write error: %s\n", uv_strerror(res));
            free(response);
            free(write_req);
        }
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

    for (uint8_t i = 0; i < match->param_count && url_params->count < url_params->capacity; i++)
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

// Context initialization
static void context_init(context_t *ctx, Arena *arena)
{
    if (!ctx || !arena)
        return;

    ctx->arena = arena;
    ctx->entries = NULL;
    ctx->count = 0;
    ctx->capacity = 0;
}

void set_context(Req *req, const char *key, void *data, size_t size)
{
    if (!req || !key || !data)
        return;

    context_t *ctx = &req->ctx;

    // Linear search for existing entry
    for (uint32_t i = 0; i < ctx->count; i++)
    {
        if (ctx->entries[i].key && strcmp(ctx->entries[i].key, key) == 0)
        {
            // Update existing entry
            ctx->entries[i].data = arena_alloc(ctx->arena, size);
            if (!ctx->entries[i].data)
                return;
            arena_memcpy(ctx->entries[i].data, data, size);
            ctx->entries[i].size = size;
            return;
        }
    }

    // Need to add new entry - check capacity
    if (ctx->count >= ctx->capacity)
    {
        uint32_t new_capacity = ctx->capacity == 0 ? 8 : ctx->capacity * 2;

        context_entry_t *new_entries = arena_realloc(ctx->arena,
                                                     ctx->entries,
                                                     ctx->capacity * sizeof(context_entry_t),
                                                     new_capacity * sizeof(context_entry_t));

        if (!new_entries)
            return;

        // Initialize new entries
        for (uint32_t i = ctx->capacity; i < new_capacity; i++)
        {
            new_entries[i].key = NULL;
            new_entries[i].data = NULL;
            new_entries[i].size = 0;
        }

        ctx->entries = new_entries;
        ctx->capacity = new_capacity;
    }

    // Add new entry
    context_entry_t *entry = &ctx->entries[ctx->count];

    entry->key = arena_strdup(ctx->arena, key);
    if (!entry->key)
        return;

    entry->data = arena_alloc(ctx->arena, size);
    if (!entry->data)
        return;

    arena_memcpy(entry->data, data, size);
    entry->size = size;

    ctx->count++;
}

void *get_context(Req *req, const char *key)
{
    if (!req || !key)
        return NULL;

    context_t *ctx = &req->ctx;

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        if (ctx->entries[i].key && strcmp(ctx->entries[i].key, key) == 0)
        {
            return ctx->entries[i].data;
        }
    }

    return NULL;
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
    context_init(&req->ctx, arena);

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

    memset(context, 0, sizeof(http_context_t));
    http_context_init(context, arena);
    return context;
}

static request_t copy_request_t(Arena *arena, const request_t *original)
{
    request_t copy;
    memset(&copy, 0, sizeof(request_t));

    if (!arena || !original || original->count == 0)
        return copy;

    // Allocate items array in arena
    copy.capacity = original->count;
    copy.count = original->count;
    copy.items = arena_alloc(arena, copy.capacity * sizeof(request_item_t));

    if (!copy.items)
    {
        memset(&copy, 0, sizeof(request_t));
        return copy;
    }

    // Copy each item using arena
    for (uint32_t i = 0; i < original->count; i++)
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
    // Early validation
    if (!res)
        return;

    if (!res->client_socket)
    {
        Arena *arena = res->arena;
        arena_free(arena);
        free(arena);
        return;
    }

    if (uv_is_closing((uv_handle_t *)res->client_socket))
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    if (!uv_is_readable((uv_stream_t *)res->client_socket) ||
        !uv_is_writable((uv_stream_t *)res->client_socket))
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    // Set defaults
    if (!content_type)
        content_type = "text/plain";
    if (!body)
        body_len = 0;

    // Get current date in HTTP format
    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    char date_str[64];
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", gmt);

    // Calculate custom headers size
    size_t headers_size = 0;
    for (uint16_t i = 0; i < res->header_count; i++)
    {
        if (res->headers[i].name && res->headers[i].value)
        {
            headers_size += strlen(res->headers[i].name) + 2 + strlen(res->headers[i].value) + 2;
        }
    }

    // Allocate header string from arena
    char *all_headers = arena_alloc(res->arena, headers_size + 1);
    if (!all_headers)
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    // Build custom headers string
    size_t pos = 0;
    for (uint16_t i = 0; i < res->header_count; i++)
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
                send_error(res->arena, res->client_socket, 500);
                return;
            }
        }
    }
    all_headers[pos] = '\0';

    // Calculate total response size
    int base_header_len = snprintf(
        NULL, 0,
        "HTTP/1.1 %d\r\n"
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
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    if (SIZE_MAX - (size_t)base_header_len < body_len)
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    size_t total_len = (size_t)base_header_len + body_len;

    // Allocate response buffer from arena
    char *response = arena_alloc(res->arena, total_len + 1);
    if (!response)
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    // Build response headers
    int written = snprintf(
        response,
        (size_t)base_header_len + 1,
        "HTTP/1.1 %d\r\n"
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

    if (written < 0 || (size_t)written > total_len)
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    // Append body if present
    if (body_len > 0 && body)
    {
        memcpy(response + written, body, body_len);
    }

    // Allocate write request
    write_req_t *write_req = arena_alloc(res->arena, sizeof(write_req_t));
    if (!write_req)
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    // Setup write request
    memset(write_req, 0, sizeof(write_req_t));
    write_req->data = response;    // Arena memory pointer
    write_req->arena = res->arena; // Transfer arena ownership
    write_req->buf = uv_buf_init(response, (unsigned int)total_len);

    // Final socket check
    if (uv_is_closing((uv_handle_t *)res->client_socket))
    {
        // Arena cleanup will be handled by callback if write succeeds
        // but we need to clean up here if we're not going to write
        Arena *arena = res->arena;
        arena_free(arena);
        free(arena);
        return;
    }

    // Perform async write
    int result = uv_write(&write_req->req, (uv_stream_t *)res->client_socket,
                          &write_req->buf, 1, write_completion_cb);

    if (result < 0)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(result));
        Arena *arena = res->arena;
        arena_free(arena);
        free(arena);
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
            send_error(NULL, client_socket, 400);
        return 1;
    }

    if (uv_is_closing((uv_handle_t *)client_socket))
        return 1;

    // Create request arena on heap
    Arena *arena = calloc(1, sizeof(Arena));
    if (!arena)
    {
        send_error(NULL, client_socket, 500);
        return 1;
    }

    // Initialize all resources
    http_context_t *ctx = NULL;
    Req *req = NULL;
    Res *res = NULL;
    tokenized_path_t tokenized_path = {0};

    // Create resources using heap arena
    ctx = create_http_context(arena);
    req = create_req(arena, client_socket);
    res = create_res(arena, client_socket);

    if (!ctx || !req || !res)
    {
        send_error(arena, client_socket, 500);
        return 1;
    }

    // Parse HTTP request
    enum llhttp_errno err = llhttp_execute(&ctx->parser, request_data, request_len);
    if (err != HPE_OK)
    {
        const char *error_name = llhttp_errno_name(err);
        const char *error_reason = llhttp_get_error_reason(&ctx->parser);

        fprintf(stderr, "HTTP parsing failed: %s", error_name);
        if (error_reason)
        {
            fprintf(stderr, " - %s", error_reason);
        }
        fprintf(stderr, "\n");

        int error_code = (err == HPE_USER) ? 400 : 500;
        send_error(arena, client_socket, error_code);
        return 1;
    }

    if (llhttp_message_needs_eof(&ctx->parser))
    {
        enum llhttp_errno finish_err = llhttp_finish(&ctx->parser);
        if (finish_err != HPE_OK)
        {
            const char *error_name = llhttp_errno_name(finish_err);
            const char *error_reason = llhttp_get_error_reason(&ctx->parser);

            fprintf(stderr, "HTTP finish failed: %s", error_name);
            if (error_reason)
            {
                fprintf(stderr, " - %s", error_reason);
            }
            fprintf(stderr, "\n");

            send_error(arena, client_socket, 400);
            return 1;
        }
    }

    // Extract path and query
    char *path = NULL;
    char *query = NULL;
    if (extract_path_and_query(arena, ctx->url, &path, &query) != 0)
    {
        send_error(arena, client_socket, 500);
        return 1;
    }

    if (!path)
    {
        send_error(arena, client_socket, 400);
        return 1;
    }

    // Parse query parameters
    parse_query(arena, query, &ctx->query_params);
    res->keep_alive = ctx->keep_alive;

    // Route matching validation
    if (!global_route_trie || !ctx->method)
    {
        printf("ERROR: Missing route trie (%p) or method (%s)\n",
               global_route_trie, ctx->method ? ctx->method : "NULL");

        // 404 but still success response: Return based on keep_alive
        int keep_alive = res->keep_alive;

        const char *not_found_msg = "404 Not Found";
        reply(res, 404, "text/plain", not_found_msg, strlen(not_found_msg));

        return keep_alive ? 0 : 1;
    }

    // Tokenize path
    if (tokenize_path(arena, path, &tokenized_path) != 0)
    {
        send_error(arena, client_socket, 500);
        return 1;
    }

    // Route matching
    route_match_t match;
    if (!route_trie_match(global_route_trie, ctx->method, &tokenized_path, &match))
    {
        // 404 but still success response: Return based on keep_alive
        int keep_alive = res->keep_alive;

        const char *not_found_msg = "404 Not Found";
        reply(res, 404, "text/plain", not_found_msg, strlen(not_found_msg));

        return keep_alive ? 0 : 1;
    }

    if (extract_url_params(arena, &match, &ctx->url_params) != 0)
    {
        send_error(arena, client_socket, 500);
        return 1;
    }

    if (populate_req_from_context(req, ctx, path) != 0)
    {
        send_error(arena, client_socket, 500);
        return 1;
    }

    if (!match.handler)
    {
        send_error(arena, client_socket, 500);
        return 1;
    }

    // Success path - call handler

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

    // SUCCESS: Return based on keep_alive setting
    // 0 = keep connection open, 1 = close connection
    return res->keep_alive ? 0 : 1;
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
        uint16_t new_cap = res->header_capacity ? res->header_capacity * 2 : 8;

        http_header_t *tmp = arena_realloc(res->arena, res->headers,
                                           res->header_capacity * sizeof(http_header_t),
                                           new_cap * sizeof(http_header_t));

        if (!tmp)
        {
            fprintf(stderr, "Error: Failed to realloc headers array\n");
            return;
        }

        memset(&tmp[res->header_capacity], 0,
               (new_cap - res->header_capacity) * sizeof(http_header_t));

        res->headers = tmp;
        res->header_capacity = new_cap;
    }

    res->headers[res->header_count].name = arena_strdup(res->arena, name);
    if (!res->headers[res->header_count].name)
    {
        fprintf(stderr, "Error: Failed to allocate memory for name\n");
        return;
    }

    res->headers[res->header_count].value = arena_strdup(res->arena, value);
    if (!res->headers[res->header_count].value)
    {
        fprintf(stderr, "Error: Failed to allocate memory for value\n");
        return;
    }

    res->header_count++;
}
