#include <stdlib.h>
#include "router.h"
#include "route-trie.h"
#include "middleware.h"
#include "server.h"
#include "arena.h"
#include "utils.h"

extern void send_error(Arena *request_arena, uv_tcp_t *client_socket, int error_code);

// Extracts URL parameters from a previously matched route
// Example: From route /users/:id matched with /users/123, extracts parameter id=123
static int extract_url_params(Arena *arena, const route_match_t *match, request_t *url_params)
{
    if (!arena || !match || !url_params)
        return -1;

    if (match->param_count == 0)
        return 0;

    url_params->capacity = match->param_count;
    url_params->count = match->param_count;
    url_params->items = arena_alloc(arena, sizeof(request_item_t) * url_params->capacity);
    if (!url_params->items)
    {
        url_params->capacity = 0;
        url_params->count = 0;
        return -1;
    }

    for (uint8_t i = 0; i < match->param_count; i++)
    {
        url_params->items[i].key = match->params[i].key;
        url_params->items[i].value = match->params[i].value;
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
    req->method = SV_NULL;
    req->path = SV_NULL;
    req->body = SV_NULL;

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

static int populate_req_from_context(Req *req, http_context_t *ctx, const char *path, size_t path_len)
{
    if (!req || !ctx)
        return -1;

    req->method.data = ctx->method;
    req->method.len = ctx->method_length;

    req->path.data = path;
    req->path.len = path_len;

    req->body.data = ctx->body;
    req->body.len = ctx->body_length;

    req->http_major = ctx->http_major;
    req->http_minor = ctx->http_minor;

    req->headers = ctx->headers;
    req->query = ctx->query_params;

    return 0;
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
    Arena *request_arena = arena_pool_acquire();
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

    const char *path = persistent_ctx->url;
    size_t path_len = persistent_ctx->path_length;

    if (!path || path_len == 0)
    {
        path = "/";
        path_len = 1;
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

    path_segment_t segments_buf[MAX_PATH_SEGMENTS];

    if (tokenize_path(path, path_len, &tokenized_path, segments_buf) != 0)
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

    if (extract_url_params(request_arena, &match, &req->params) != 0)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    if (populate_req_from_context(req, persistent_ctx, path, path_len) != 0)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    if (!match.handler)
    {
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

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
        send_error(request_arena, (uv_tcp_t *)&client->handle, 500);
        return 1;
    }

    if (!res->replied)
        return 0;

    return res->keep_alive ? 0 : 1;
}
