#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "ecewo.h"
#include "router.h"
#include "route_trie.h"
#include "middleware.h"
#include "client.h"

// Forward declaration for client structure
typedef struct client_s client_t;

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
            Arena *request_arena = write_req->arena;
            arena_free(request_arena);
            free(request_arena);
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
static void send_error(Arena *request_arena, uv_tcp_t *client_socket, int error_code)
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

    if (request_arena)
    {
        // Arena path
        char *response = arena_sprintf(request_arena,
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
            arena_free(request_arena);
            free(request_arena);
            return;
        }

        size_t response_len = strlen(response);

        write_req_t *write_req = arena_alloc(request_arena, sizeof(write_req_t));
        if (!write_req)
        {
            arena_free(request_arena);
            free(request_arena);
            return;
        }

        memset(&write_req->req, 0, sizeof(uv_write_t));
        write_req->data = response;
        write_req->arena = request_arena;
        write_req->buf = uv_buf_init(response, (unsigned int)response_len);

        int res = uv_write(&write_req->req, (uv_stream_t *)client_socket,
                           &write_req->buf, 1, write_completion_cb);
        if (res < 0)
        {
            fprintf(stderr, "Write error: %s\n", uv_strerror(res));
            arena_free(request_arena);
            free(request_arena);
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
static int extract_path_and_query(Arena *request_arena, char *url_buf, char **path, char **query)
{
    if (!request_arena || !url_buf || !path || !query)
        return -1;

    char *qmark = strchr(url_buf, '?');
    if (qmark)
    {
        *qmark = '\0';
        *path = arena_strdup(request_arena, url_buf);
        *query = arena_strdup(request_arena, qmark + 1);
    }
    else
    {
        *path = arena_strdup(request_arena, url_buf);
        *query = arena_strdup(request_arena, "");
    }

    if (!*path || !*query)
        return -1;

    // If path is empty, treat it as root
    if ((*path)[0] == '\0')
    {
        *path = arena_strdup(request_arena, "/");
        if (!*path)
            return -1;
    }
    return 0;
}

// Extracts URL parameters from a previously matched route
// Example: From route /users/:id matched with /users/123, extracts parameter id=123
static int extract_url_params(Arena *request_arena, const route_match_t *match, request_t *url_params)
{
    if (!request_arena || !match || !url_params)
        return -1;

    if (url_params->capacity == 0)
    {
        url_params->capacity = match->param_count > 0 ? match->param_count : 1;
        url_params->items = arena_alloc(request_arena, sizeof(request_item_t) * url_params->capacity);
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
        char *key = arena_alloc(request_arena, match->params[i].key.len + 1);
        char *value = arena_alloc(request_arena, match->params[i].value.len + 1);

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
static void context_init(context_t *ctx, Arena *request_arena)
{
    if (!ctx || !request_arena)
        return;

    ctx->arena = request_arena;
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
static Req *create_req(Arena *request_arena, uv_tcp_t *client_socket)
{
    if (!request_arena)
        return NULL;

    Req *req = arena_alloc(request_arena, sizeof(Req));
    if (!req)
        return NULL;

    memset(req, 0, sizeof(Req));
    req->arena = request_arena;
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
    context_init(&req->ctx, request_arena);

    return req;
}

// Create and initialize Res
static Res *create_res(Arena *request_arena, uv_tcp_t *client_socket)
{
    if (!request_arena)
        return NULL;

    Res *res = arena_alloc(request_arena, sizeof(Res));
    if (!res)
        return NULL;

    memset(res, 0, sizeof(Res));
    res->arena = request_arena;
    res->client_socket = client_socket;
    res->status = 200;
    res->content_type = arena_strdup(request_arena, "text/plain");
    res->body = NULL;
    res->body_len = 0;
    res->keep_alive = 1;
    res->headers = NULL;
    res->header_count = 0;
    res->header_capacity = 0;

    return res;
}

// Copy request_t from connection arena to request arena
static request_t copy_request_t(Arena *request_arena, const request_t *original)
{
    request_t copy;
    memset(&copy, 0, sizeof(request_t));

    if (!request_arena || !original || original->count == 0)
        return copy;

    // Allocate items array in request arena
    copy.capacity = original->count;
    copy.count = original->count;
    copy.items = arena_alloc(request_arena, copy.capacity * sizeof(request_item_t));

    if (!copy.items)
    {
        memset(&copy, 0, sizeof(request_t));
        return copy;
    }

    // Copy each item using request arena
    for (uint32_t i = 0; i < original->count; i++)
    {
        if (original->items[i].key)
        {
            copy.items[i].key = arena_strdup(request_arena, original->items[i].key);
            if (!copy.items[i].key)
            {
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
            copy.items[i].value = arena_strdup(request_arena, original->items[i].value);
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

// Populate request from persistent context - copy from connection arena to request arena
static int populate_req_from_context(Req *req, http_context_t *persistent_ctx, const char *path)
{
    if (!req || !req->arena || !persistent_ctx)
        return -1;

    Arena *request_arena = req->arena;

    // Copy method from persistent context to request arena
    if (persistent_ctx->method)
    {
        req->method = arena_strdup(request_arena, persistent_ctx->method);
        if (!req->method)
            return -1;
    }

    // Copy path to request arena
    if (path)
    {
        req->path = arena_strdup(request_arena, path);
        if (!req->path)
            return -1;
    }

    // Copy body from persistent context to request arena
    if (persistent_ctx->body && persistent_ctx->body_length > 0)
    {
        req->body = arena_alloc(request_arena, persistent_ctx->body_length + 1);
        if (!req->body)
            return -1;
        arena_memcpy(req->body, persistent_ctx->body, persistent_ctx->body_length);
        req->body[persistent_ctx->body_length] = '\0';
        req->body_len = persistent_ctx->body_length;
    }

    // Copy containers from connection arena to request arena
    req->headers = copy_request_t(request_arena, &persistent_ctx->headers);
    req->query = copy_request_t(request_arena, &persistent_ctx->query_params);
    req->params = copy_request_t(request_arena, &persistent_ctx->url_params);

    return 0;
}

// Composes and sends the response
void reply(Res *res, int status, const char *content_type, const void *body, size_t body_len)
{
    // Early validation
    if (!res)
        return;

    if (!res->client_socket)
    {
        Arena *request_arena = res->arena;
        arena_free(request_arena);
        free(request_arena);
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

    // Allocate header string from request arena
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

    // Allocate response buffer from request arena
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

    // Allocate write request in request arena
    write_req_t *write_req = arena_alloc(res->arena, sizeof(write_req_t));
    if (!write_req)
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    // Setup write request
    memset(write_req, 0, sizeof(write_req_t));
    write_req->data = response;    // Request arena memory pointer
    write_req->arena = res->arena; // Transfer request arena ownership
    write_req->buf = uv_buf_init(response, (unsigned int)total_len);

    // Final socket check
    if (uv_is_closing((uv_handle_t *)res->client_socket))
    {
        // Arena cleanup will be handled by callback if write succeeds
        // but we need to clean up here if we're not going to write
        Arena *request_arena = res->arena;
        arena_free(request_arena);
        free(request_arena);
        return;
    }

    // Perform async write
    int result = uv_write(&write_req->req, (uv_stream_t *)res->client_socket,
                          &write_req->buf, 1, write_completion_cb);

    if (result < 0)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(result));
        Arena *request_arena = res->arena;
        arena_free(request_arena);
        free(request_arena);
        return;
    }
}

// Main router function
int router(client_t *client, const char *request_data, size_t request_len)
{
    // Early validation
    if (!client || !request_data || request_len == 0)
    {
        if (client && client->handle.data)
            send_error(NULL, (uv_tcp_t *)&client->handle, 400);
        return 1;
    }

    if (uv_is_closing((uv_handle_t *)&client->handle))
        return 1;

    // Get persistent context from client (connection arena)
    http_context_t *persistent_ctx = &client->persistent_context;

    // Create request arena (separate from connection arena)
    Arena *request_arena = calloc(1, sizeof(Arena));
    if (!request_arena)
    {
        send_error(NULL, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    // Initialize resources in request arena
    Req *req = NULL;
    Res *res = NULL;
    tokenized_path_t tokenized_path = {0};

    // Create resources using request arena
    req = create_req(request_arena, (uv_tcp_t *)&client->handle);
    res = create_res(request_arena, (uv_tcp_t *)&client->handle);

    if (!req || !res)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    // Parse request using persistent parser and context
    parse_result_t parse_result = http_parse_request(persistent_ctx, request_data, request_len);

    switch (parse_result)
    {
    case PARSE_SUCCESS:
        break;

    case PARSE_INCOMPLETE:
        fprintf(stderr, "HTTP parsing incomplete - need more data\n");
        send_error(request_arena, (uv_tcp_t *)&client->handle, 400);
        return 1;

    case PARSE_OVERFLOW:
        fprintf(stderr, "HTTP parsing failed: size limits exceeded\n");
        if (persistent_ctx->error_reason)
            fprintf(stderr, " - %s\n", persistent_ctx->error_reason);
        send_error(request_arena, (uv_tcp_t *)&client->handle, 413);
        return 1;

    case PARSE_ERROR:
    default:
        fprintf(stderr, "HTTP parsing failed: %s\n", parse_result_to_string(parse_result));
        if (persistent_ctx->error_reason)
            fprintf(stderr, " - %s\n", persistent_ctx->error_reason);
        send_error(request_arena, (uv_tcp_t *)&client->handle, 400);
        return 1;
    }

    // Check if we need to finish parsing
    if (http_message_needs_eof(persistent_ctx))
    {
        parse_result_t finish_result = http_finish_parsing(persistent_ctx);
        if (finish_result != PARSE_SUCCESS)
        {
            fprintf(stderr, "HTTP finish parsing failed: %s\n",
                    parse_result_to_string(finish_result));
            if (persistent_ctx->error_reason)
                fprintf(stderr, " - %s\n", persistent_ctx->error_reason);
            send_error(request_arena, (uv_tcp_t *)&client->handle, 400);
            return 1;
        }
    }

    res->keep_alive = persistent_ctx->keep_alive;

    // Extract path and query in request arena
    char *path = NULL;
    char *query = NULL;
    if (extract_path_and_query(request_arena, persistent_ctx->url, &path, &query) != 0)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    if (!path)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 400);
        return 1;
    }

    // Route matching validation
    if (!global_route_trie || !persistent_ctx->method)
    {
        fprintf(stderr, "ERROR: Missing route trie (%p) or method (%s)\n",
                (void *)global_route_trie,
                persistent_ctx->method ? persistent_ctx->method : "NULL");

        // 404 but still success response: Return based on keep_alive
        bool keep_alive = res->keep_alive;
        const char *not_found_msg = "404 Not Found";
        reply(res, 404, "text/plain", not_found_msg, strlen(not_found_msg));
        return keep_alive ? 0 : 1;
    }

    // Tokenize path in request arena
    if (tokenize_path(request_arena, path, &tokenized_path) != 0)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    // Route matching
    route_match_t match;
    if (!route_trie_match(global_route_trie, persistent_ctx->parser, &tokenized_path, &match))
    {
        bool keep_alive = res->keep_alive;
        const char *not_found_msg = "404 Not Found";
        reply(res, 404, "text/plain", not_found_msg, strlen(not_found_msg));
        return keep_alive ? 0 : 1;
    }

    // Extract URL parameters in request arena
    if (extract_url_params(request_arena, &match, &persistent_ctx->url_params) != 0)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    // Populate request from persistent context (copy to request arena)
    if (populate_req_from_context(req, persistent_ctx, path) != 0)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    if (!match.handler)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    // ============================================================================
    // ASYNC/SYNC HANDLER EXECUTION
    // ============================================================================

    // Check if this is an async handler
    MiddlewareInfo *middleware_info = (MiddlewareInfo *)match.middleware_ctx;
    handler_type_t handler_type = HANDLER_SYNC; // Default to sync

    // Determine handler type from middleware info
    if (middleware_info)
    {
        handler_type = middleware_info->handler_type;
    }

    if (handler_type == HANDLER_ASYNC)
    {
        // ========================================================================
        // ASYNCHRONOUS EXECUTION (Thread Pool)
        // ========================================================================

        if (middleware_info)
        {
            // TODO: Full async middleware chain support
            // For now, execute middleware synchronously then handler async
            // This is a simplified implementation that covers most use cases

            // Execute global and route-specific middleware synchronously
            int total_middleware_count = global_middleware_count + middleware_info->middleware_count;

            if (total_middleware_count > 0)
            {
                // Allocate combined middleware handlers
                MiddlewareHandler *combined_handlers = arena_alloc(req->arena,
                                                                   sizeof(MiddlewareHandler) * total_middleware_count);
                if (!combined_handlers)
                {
                    send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
                    return 1;
                }

                // Copy global middleware handlers first
                arena_memcpy(combined_handlers, global_middleware, sizeof(MiddlewareHandler) * global_middleware_count);

                // Copy route-specific middleware handlers
                if (middleware_info->middleware_count > 0 && middleware_info->middleware)
                {
                    arena_memcpy(combined_handlers + global_middleware_count, middleware_info->middleware,
                                 sizeof(MiddlewareHandler) * middleware_info->middleware_count);
                }

                // Create middleware chain context
                Chain *chain = arena_alloc(req->arena, sizeof(Chain));
                if (!chain)
                {
                    send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
                    return 1;
                }

                chain->handlers = combined_handlers;
                chain->count = total_middleware_count;
                chain->current = 0;
                chain->route_handler = middleware_info->handler;
                chain->handler_type = handler_type;

                // Execute middleware chain (will end with async handler execution)
                int result = next(req, res, chain);
                if (result == -1)
                {
                    // Middleware chain failed, try handler directly
                    if (execute_async_handler(middleware_info->handler, req, res) != 0)
                    {
                        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
                        return 1;
                    }
                }
            }
            else
            {
                // No middleware, execute handler directly in thread pool
                if (execute_async_handler(middleware_info->handler, req, res) != 0)
                {
                    send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
                    return 1;
                }
            }
        }
        else
        {
            // No middleware info, execute handler directly in thread pool
            if (execute_async_handler(match.handler, req, res) != 0)
            {
                send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
                return 1;
            }
        }

        // Return immediately - async handler will complete later
        // Arena cleanup will be handled by async callback
        // Connection handling based on keep_alive setting
        return res->keep_alive ? 0 : 1;
    }
    else
    {
        // ========================================================================
        // SYNCHRONOUS EXECUTION (Main Thread) - Original Behavior
        // ========================================================================

        if (middleware_info)
        {
            execute_middleware_chain(req, res, middleware_info);
        }
        else
        {
            match.handler(req, res);
        }

        // SUCCESS: Return based on keep_alive setting
        // Arena cleanup handled by write_completion_cb
        // 0 = keep connection open, 1 = close connection
        return res->keep_alive ? 0 : 1;
    }
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

static void async_handler_work(uv_work_t *req)
{
    async_handler_context_t *ctx = (async_handler_context_t *)req->data;

    if (!ctx || !ctx->handler || !ctx->req || !ctx->res)
    {
        if (ctx)
        {
            ctx->error_message = "Invalid async handler context";
            ctx->completed = false;
        }
        return;
    }

    // Check if server is shutting down
    if (!server_is_running())
    {
        ctx->error_message = "Server is shutting down";
        ctx->completed = false;
        return;
    }

    // Execute handler in thread pool
    ctx->handler(ctx->req, ctx->res);
    ctx->completed = true;
    ctx->error_message = NULL;
}

static void async_handler_after_work(uv_work_t *req, int status)
{
    async_handler_context_t *ctx = (async_handler_context_t *)req->data;

    if (!ctx)
        return;

    // Check libuv work status
    if (status < 0)
    {
        fprintf(stderr, "Async handler work failed: %s\n", uv_strerror(status));
        // Direkt erişim
        send_error(ctx->req->arena, ctx->req->client_socket, 500);
        return;
    }

    // Check handler execution status
    if (!ctx->completed || ctx->error_message)
    {
        fprintf(stderr, "Handler execution failed: %s\n",
                ctx->error_message ? ctx->error_message : "Unknown error");
        // Direkt erişim
        send_error(ctx->req->arena, ctx->req->client_socket, 500);
        return;
    }

    // Success: response should have been sent by handler
    // Arena cleanup is handled by write_completion_cb
}

int execute_async_handler(RequestHandler handler, Req *req, Res *res)
{
    if (!handler || !req || !res)
        return -1;

    // Create context from arena
    async_handler_context_t *ctx = arena_alloc(req->arena, sizeof(async_handler_context_t));
    if (!ctx)
        return -1;

    // Initialize - minimal fields
    ctx->work_req.data = ctx;
    ctx->handler = handler;
    ctx->req = req;
    ctx->res = res;
    ctx->completed = false;
    ctx->error_message = NULL;

    // Queue work in thread pool
    int result = uv_queue_work(
        uv_default_loop(),
        &ctx->work_req,
        async_handler_work,
        async_handler_after_work);

    return result == 0 ? 0 : -1;
}

const char *get_param(const Req *req, const char *key)
{
    return req ? get_req(&req->params, key) : NULL;
}

const char *get_query(const Req *req, const char *key)
{
    return req ? get_req(&req->query, key) : NULL;
}

const char *get_header(const Req *req, const char *key)
{
    return req ? get_req(&req->headers, key) : NULL;
}

void redirect(Res *res, int status, const char *url)
{
    if (!res || !url)
        return;

    set_header(res, "Location", url);

    const char *message;
    size_t message_len;

    switch (status)
    {
    case MOVED_PERMANENTLY:
        message = "Moved Permanently";
        message_len = 17;
        break;
    case FOUND:
        message = "Found";
        message_len = 5;
        break;
    case SEE_OTHER:
        message = "See Other";
        message_len = 9;
        break;
    case TEMPORARY_REDIRECT:
        message = "Temporary Redirect";
        message_len = 18;
        break;
    case PERMANENT_REDIRECT:
        message = "Permanent Redirect";
        message_len = 18;
        break;
    default:
        message = "Redirect";
        message_len = 8;
        break;
    }

    reply(res, status, "text/plain", message, message_len);
}
