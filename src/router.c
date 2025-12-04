#include <stdlib.h>
#include "router.h"
#include "route-trie.h"
#include "middleware.h"
#include "server.h"

// Called when write operation is completed
static void write_completion_cb(uv_write_t *req, int status)
{
    if (status < 0)
        LOG_ERROR("Write error: %s", uv_strerror(status));

    write_req_t *write_req = (write_req_t *)req;
    if (!write_req)
        return;

    if (write_req->async_context)
    {
        // Async route
        // Arena will be cleaned up in cleanup_async_context
        uv_async_t *async_handle = get_async_handle_from_context(write_req->async_context);
        if (async_handle)
            uv_close((uv_handle_t *)async_handle, cleanup_async_context);
    }
    else if (write_req->arena)
    {
        // Sync route
        Arena *request_arena = write_req->arena;
        arena_free(request_arena);
        free(request_arena);
    }
    else
    {
        // Early error path
        if (write_req->data)
        {
            free(write_req->data);
            write_req->data = NULL;
        }
        memset(write_req, 0, sizeof(write_req_t));
        free(write_req);
    }
}

// Sends 400 or 500
static void send_error(Arena *request_arena, uv_tcp_t *client_socket, int error_code)
{
    if (!client_socket)
        return;

    if (uv_is_closing((uv_handle_t *)client_socket))
        return;

    if (!uv_is_readable((uv_stream_t *)client_socket) ||
        !uv_is_writable((uv_stream_t *)client_socket))
        return;

    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    char date_str[64];
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", gmt);

    const char *status_text = (error_code == 500) ? "Internal Server Error" : "Bad Request";
    const char *body = status_text;
    size_t body_len = strlen(body);

    if (request_arena)
    {
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
            LOG_DEBUG("Write error: %s", uv_strerror(res));
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
            LOG_DEBUG("Write error: %s", uv_strerror(res));
            free(response);
            free(write_req);
        }
    }
}

// Separates URL into path and query string components
// Example: /users/123?active=true -> path="/users/123", query="active=true"
static int extract_path_and_query(Arena *request_arena, const char *url_buf, char **path, char **query)
{
    if (!request_arena || !url_buf || !path || !query)
        return -1;

    const char *qmark = strchr(url_buf, '?');
    if (qmark)
    {
        size_t path_len = qmark - url_buf;
        *path = arena_alloc(request_arena, path_len + 1);
        if (!*path)
            return -1;
        
        memcpy(*path, url_buf, path_len);
        (*path)[path_len] = '\0';
        
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
            return -1;

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

    for (uint32_t i = 0; i < ctx->count; i++)
    {
        if (ctx->entries[i].key && strcmp(ctx->entries[i].key, key) == 0)
        {
            ctx->entries[i].data = arena_alloc(ctx->arena, size);
            if (!ctx->entries[i].data)
                return;
            arena_memcpy(ctx->entries[i].data, data, size);
            ctx->entries[i].size = size;
            return;
        }
    }

    if (ctx->count >= ctx->capacity)
    {
        uint32_t new_capacity = ctx->capacity == 0 ? 8 : ctx->capacity * 2;

        context_entry_t *new_entries = arena_realloc(ctx->arena,
                                                     ctx->entries,
                                                     ctx->capacity * sizeof(context_entry_t),
                                                     new_capacity * sizeof(context_entry_t));

        if (!new_entries)
            return;

        for (uint32_t i = ctx->capacity; i < new_capacity; i++)
        {
            new_entries[i].key = NULL;
            new_entries[i].data = NULL;
            new_entries[i].size = 0;
        }

        ctx->entries = new_entries;
        ctx->capacity = new_capacity;
    }

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
            return ctx->entries[i].data;
    }

    return NULL;
}

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

    memset(&req->headers, 0, sizeof(request_t));
    memset(&req->query, 0, sizeof(request_t));
    memset(&req->params, 0, sizeof(request_t));

    context_init(&req->ctx, request_arena);

    return req;
}

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
    res->replied = false;

    return res;
}

static request_t copy_request_t(Arena *request_arena, const request_t *original)
{
    request_t copy;
    memset(&copy, 0, sizeof(request_t));

    if (!request_arena || !original || original->count == 0)
        return copy;

    copy.capacity = original->count;
    copy.count = original->count;
    copy.items = arena_alloc(request_arena, copy.capacity * sizeof(request_item_t));

    if (!copy.items)
    {
        memset(&copy, 0, sizeof(request_t));
        return copy;
    }

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

// Copy the persistent context from connection arena to request arena
static int populate_req_from_context(Req *req, http_context_t *persistent_ctx, const char *path)
{
    if (!req || !req->arena || !persistent_ctx)
        return -1;

    Arena *request_arena = req->arena;

    if (persistent_ctx->method)
    {
        req->method = arena_strdup(request_arena, persistent_ctx->method);
        if (!req->method)
            return -1;
    }

    if (path)
    {
        req->path = arena_strdup(request_arena, path);
        if (!req->path)
            return -1;
    }

    if (persistent_ctx->body && persistent_ctx->body_length > 0)
    {
        req->body = arena_alloc(request_arena, persistent_ctx->body_length + 1);
        if (!req->body)
            return -1;
        arena_memcpy(req->body, persistent_ctx->body, persistent_ctx->body_length);
        req->body[persistent_ctx->body_length] = '\0';
        req->body_len = persistent_ctx->body_length;
    }

    req->headers = copy_request_t(request_arena, &persistent_ctx->headers);
    req->query = copy_request_t(request_arena, &persistent_ctx->query_params);
    req->params = copy_request_t(request_arena, &persistent_ctx->url_params);

    return 0;
}

void reply(Res *res, int status, const char *content_type, const void *body, size_t body_len)
{
    if (!res)
        return;

    res->replied = true;

    if (res->async_buffer)
    {
        async_response_buffer_t *buffer = (async_response_buffer_t *)res->async_buffer;
        
        buffer->status_code = status;
        buffer->content_type = content_type ? arena_strdup(res->arena, content_type) : NULL;
        
        if (body && body_len > 0)
        {
            buffer->response_body = arena_alloc(res->arena, body_len);
            if (buffer->response_body)
            {
                arena_memcpy(buffer->response_body, body, body_len);
                buffer->response_body_len = body_len;
            }
        }
        
        buffer->response_ready = true;
        uv_async_send(&buffer->async_send);
        return;
    }

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

    if (!content_type)
        content_type = "text/plain";
    if (!body)
        body_len = 0;

    time_t now = time(NULL);
    struct tm *gmt = gmtime(&now);
    char date_str[64];
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", gmt);

    size_t headers_size = 0;
    for (uint16_t i = 0; i < res->header_count; i++)
    {
        if (res->headers[i].name && res->headers[i].value)
        {
            headers_size += strlen(res->headers[i].name) + 2 + strlen(res->headers[i].value) + 2;
        }
    }

    char *all_headers = arena_alloc(res->arena, headers_size + 1);
    if (!all_headers)
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

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

    char *response = arena_alloc(res->arena, total_len + 1);
    if (!response)
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

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

    if (body_len > 0 && body)
    {
        memcpy(response + written, body, body_len);
    }

    write_req_t *write_req = arena_alloc(res->arena, sizeof(write_req_t));
    if (!write_req)
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    memset(write_req, 0, sizeof(write_req_t));
    write_req->data = response;
    write_req->async_context = res->async_context;
    write_req->arena = res->async_context ? NULL : res->arena;
    write_req->buf = uv_buf_init(response, (unsigned int)total_len);

    if (uv_is_closing((uv_handle_t *)res->client_socket))
    {
        // Arena cleanup will be handled by callback if write succeeds
        // but we need to clean up here if we're not going to write
        Arena *request_arena = res->arena;
        arena_free(request_arena);
        free(request_arena);
        return;
    }

    int result = uv_write(&write_req->req, (uv_stream_t *)res->client_socket,
                          &write_req->buf, 1, write_completion_cb);

    if (result < 0)
    {
        LOG_DEBUG("Write error: %s", uv_strerror(result));
        Arena *request_arena = res->arena;
        arena_free(request_arena);
        free(request_arena);
        return;
    }
}

// Return values:
//   0 = success, keep connection open (keep-alive)
//   1 = close connection (error or Connection: close)
//   2 = need more data (PARSE_INCOMPLETE)
int router(client_t *client, const char *request_data, size_t request_len)
{
    if (!client || !request_data || request_len == 0)
    {
        if (client && client->handle.data)
            send_error(NULL, (uv_tcp_t *)&client->handle, 400);
        return 1;
    }

    if (uv_is_closing((uv_handle_t *)&client->handle))
        return 1;

    http_context_t *persistent_ctx = &client->persistent_context;

    // Parse the incoming data (appends to existing parsed data if partial)
    parse_result_t parse_result = http_parse_request(persistent_ctx, request_data, request_len);

    switch (parse_result)
    {
    case PARSE_SUCCESS:
        break;

    case PARSE_INCOMPLETE:
        // Need more data - don't create arena, don't send error
        // Just return and wait for more data
        LOG_DEBUG("HTTP parsing incomplete - waiting for more data");
        return 2;

    case PARSE_OVERFLOW:
        LOG_DEBUG("HTTP parsing failed: size limits exceeded");

        if (persistent_ctx->error_reason)
            LOG_DEBUG(" - %s", persistent_ctx->error_reason);
        send_error(NULL, (uv_tcp_t *)&client->handle, 413);
        return 1;

    case PARSE_ERROR:
    default:
        LOG_DEBUG("HTTP parsing failed: %s", parse_result_to_string(parse_result));
        if (persistent_ctx->error_reason)
            LOG_DEBUG(" - %s", persistent_ctx->error_reason);
        send_error(NULL, (uv_tcp_t *)&client->handle, 400);
        return 1;
    }

    // From here on, we have a complete HTTP message
    // Create request arena for processing
    Arena *request_arena = calloc(1, sizeof(Arena));
    if (!request_arena)
    {
        send_error(NULL, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    Req *req = NULL;
    Res *res = NULL;
    tokenized_path_t tokenized_path = {0};

    req = create_req(request_arena, (uv_tcp_t *)&client->handle);
    res = create_res(request_arena, (uv_tcp_t *)&client->handle);

    if (!req || !res)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    // Check if we need to finish parsing
    if (http_message_needs_eof(persistent_ctx))
    {
        parse_result_t finish_result = http_finish_parsing(persistent_ctx);
        if (finish_result != PARSE_SUCCESS)
        {
            LOG_DEBUG("HTTP finish parsing failed: %s", parse_result_to_string(finish_result));

            if (persistent_ctx->error_reason)
                LOG_DEBUG(" - %s", persistent_ctx->error_reason);

            send_error(request_arena, (uv_tcp_t *)&client->handle, 400);
            return 1;
        }
    }

    res->keep_alive = persistent_ctx->keep_alive;

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

    if (!global_route_trie || !persistent_ctx->method)
    {
        LOG_DEBUG("Missing route trie (%p) or method (%s)",
                 (void *)global_route_trie,
                 persistent_ctx->method ? persistent_ctx->method : "NULL"
        );

        // 404 but still success response
        bool keep_alive = res->keep_alive;
        const char *not_found_msg = "404 Not Found";
        reply(res, 404, "text/plain", not_found_msg, strlen(not_found_msg));
        return keep_alive ? 0 : 1;
    }

    if (tokenize_path(request_arena, path, &tokenized_path) != 0)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    route_match_t match;
    if (!route_trie_match(global_route_trie, persistent_ctx->parser, &tokenized_path, &match))
    {
        LOG_DEBUG("Route not found: %s %s", persistent_ctx->method, path);

        bool keep_alive = res->keep_alive;
        const char *not_found_msg = "404 Not Found";
        reply(res, 404, "text/plain", not_found_msg, strlen(not_found_msg));
        return keep_alive ? 0 : 1;
    }

    if (extract_url_params(request_arena, &match, &persistent_ctx->url_params) != 0)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

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
    // EXECUTION - middleware.c handles everything (sync and async)
    // ============================================================================

    MiddlewareInfo *middleware_info = (MiddlewareInfo *)match.middleware_ctx;

    if (!middleware_info)
    {
        LOG_DEBUG("No middleware info");
        match.handler(req, res);
        return res->keep_alive ? 0 : 1;
    }

    int execution_result = execute_handler_with_middleware(req, res, middleware_info);

    if (execution_result != 0)
    {
        LOG_ERROR("Handler execution failed");
        // Handler execution failed
        if (middleware_info->handler_type == HANDLER_ASYNC)
        {
            LOG_ERROR("Async execution failed");
            return 1;
        }
        else
        {
            LOG_ERROR("Sync handler failed");
            send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
            return 1;
        }
    }

    if (middleware_info->handler_type == HANDLER_ASYNC)
    {
        // Async handler - don't close connection yet
        // Callback will handle response and connection management
        return 0;
    }

    if (!res->replied)
        return 0;

    return res->keep_alive ? 0 : 1;
}

void set_header(Res *res, const char *name, const char *value)
{
    if (!res || !res->arena || !name || !value)
    {
        LOG_DEBUG("Invalid argument(s) to set_header");
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
            LOG_DEBUG("Failed to realloc headers array");
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
        LOG_DEBUG("Failed to allocate memory for name in set_header");
        return;
    }

    res->headers[res->header_count].value = arena_strdup(res->arena, value);
    if (!res->headers[res->header_count].value)
    {
        LOG_DEBUG("Failed to allocate memory for value in set_header");
        return;
    }

    res->header_count++;
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

static void spawn_cleanup_cb(uv_handle_t *handle)
{
    spawn_t *t = (spawn_t *)handle->data;
    if (t)
        free(t);
}

static void spawn_async_cb(uv_async_t *handle)
{
    spawn_t *t = (spawn_t *)handle->data;
    if (!t)
        return;

    if (t->result_fn)
        t->result_fn(t->context);

    decrement_async_work();
    uv_close((uv_handle_t *)handle, spawn_cleanup_cb);
}

static void spawn_work_cb(uv_work_t *req)
{
    spawn_t *t = (spawn_t *)req->data;
    if (t && t->work_fn)
        t->work_fn(t->context);
}

static void spawn_after_work_cb(uv_work_t *req, int status)
{
    spawn_t *t = (spawn_t *)req->data;
    if (!t)
        return;

    if (status < 0)
        LOG_ERROR("Spawn execution failed");

    uv_async_send(&t->async_send);
}

int spawn(void *context, spawn_handler_t work_fn, spawn_handler_t done_fn)
{
    if (!context || !work_fn)
        return -1;

    spawn_t *task = calloc(1, sizeof(spawn_t));
    if (!task)
        return -1;

    if (uv_async_init(uv_default_loop(), &task->async_send, spawn_async_cb) != 0)
    {
        free(task);
        return -1;
    }

    task->work.data = task;
    task->async_send.data = task;
    task->context = context;
    task->work_fn = work_fn;
    task->result_fn = done_fn;

    increment_async_work();

    int result = uv_queue_work(
        uv_default_loop(),
        &task->work,
        spawn_work_cb,
        spawn_after_work_cb);

    if (result != 0)
    {
        uv_close((uv_handle_t *)&task->async_send, NULL);
        decrement_async_work();
        free(task);
        return result;
    }

    return 0;
}
