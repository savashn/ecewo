#include "uv.h"
#include "arena.h"
#include "utils.h"
#include <stdlib.h>

typedef struct
{
    uv_write_t req;
    uv_buf_t buf;
    char *data;
    Arena *arena;
} write_req_t;

static void write_completion_cb(uv_write_t *req, int status)
{
    if (status < 0)
        LOG_ERROR("Write error: %s", uv_strerror(status));

    write_req_t *write_req = (write_req_t *)req;
    if (!write_req)
        return;

    if (write_req->arena)
    {
        Arena *request_arena = write_req->arena;
        arena_pool_release(request_arena);
    }
    else
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

// Sends 400 or 500
void send_error(Arena *request_arena, uv_tcp_t *client_socket, int error_code)
{
    if (!client_socket)
        return;

    if (uv_is_closing((uv_handle_t *)client_socket))
        return;

    if (!uv_is_readable((uv_stream_t *)client_socket) ||
        !uv_is_writable((uv_stream_t *)client_socket))
        return;

    const char *date_str = get_cached_date();

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
            arena_pool_release(request_arena);
            return;
        }

        size_t response_len = strlen(response);

        write_req_t *write_req = arena_alloc(request_arena, sizeof(write_req_t));
        if (!write_req)
        {
            arena_pool_release(request_arena);
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
            arena_pool_release(request_arena);
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

void reply(Res *res, int status, const char *content_type, const void *body, size_t body_len)
{
    if (!res)
        return;

    res->replied = true;

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

    const char *date_str = get_cached_date();

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
    write_req->arena = res->arena;
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
