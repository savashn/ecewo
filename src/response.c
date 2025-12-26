#include "uv.h"
#include "arena.h"
#include "utils.h"
#include "logger.h"
#include "server.h"
#include <stdlib.h>
#include <ctype.h>

#ifdef ECEWO_DEBUG
    #ifdef _WIN32
        #define strcasecmp _stricmp
    #else
        #define strcasecmp strcasecmp
    #endif
#endif

typedef struct
{
    uv_write_t req;
    uv_buf_t buf;
    char *data;
    Arena *arena;
    client_t *client;
} write_req_t;

static void end_request(client_t *client)
{
    client->request_in_progress = false;
        
    if (client->request_timeout_timer)
    {
        uv_timer_stop(client->request_timeout_timer);
        uv_close((uv_handle_t *)client->request_timeout_timer, (uv_close_cb)free);
        client->request_timeout_timer = NULL;
    }
}

static void write_completion_cb(uv_write_t *req, int status)
{
    if (status < 0)
        LOG_ERROR("Write error: %s", uv_strerror(status));

    write_req_t *write_req = (write_req_t *)req;
    if (!write_req)
        return;

    if (write_req->client)
        end_request(write_req->client);

    if (write_req->arena)
    {
        arena_reset(write_req->arena);
    }
    else
    {
        // Malloc fallback
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
void send_error(Arena *arena, uv_tcp_t *client_socket, int error_code)
{
    if (!client_socket)
    {
        if (arena)
            arena_reset(arena);
        return;
    }

    if (uv_is_closing((uv_handle_t *)client_socket))
    {
        if (arena)
            arena_reset(arena);
        return;
    }

    if (!uv_is_readable((uv_stream_t *)client_socket) ||
        !uv_is_writable((uv_stream_t *)client_socket))
    {
        if (arena)
            arena_reset(arena);
        return;
    }

    const char *date_str = get_cached_date();

    const char *status_text = (error_code == 500) ? "Internal Server Error" : "Bad Request";
    const char *body = status_text;
    size_t body_len = strlen(body);

    if (arena)
    {
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
            arena_reset(arena);
            return;
        }

        size_t response_len = strlen(response);

        write_req_t *write_req = arena_alloc(arena, sizeof(write_req_t));
        if (!write_req)
        {
            arena_reset(arena);
            return;
        }

        memset(&write_req->req, 0, sizeof(uv_write_t));
        write_req->data = response;
        write_req->arena = arena;
        write_req->client = (client_t *)client_socket->data;

        write_req->buf = uv_buf_init(response, (unsigned int)response_len);

        int res = uv_write(&write_req->req, (uv_stream_t *)client_socket,
                           &write_req->buf, 1, write_completion_cb);
        if (res < 0)
        {
            LOG_ERROR("Write error: %s", uv_strerror(res));
            arena_reset(arena);

            if (write_req->client)
                end_request(write_req->client);
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
        write_req->client = (client_t *)client_socket->data;
        write_req->buf = uv_buf_init(response, (unsigned int)written);

        int res = uv_write(&write_req->req, (uv_stream_t *)client_socket,
                           &write_req->buf, 1, write_completion_cb);
        if (res < 0)
        {
            LOG_ERROR("Write error: %s", uv_strerror(res));
            client_t *client = write_req->client;

            free(response);
            free(write_req);

            if (client)
                end_request(client);
        }
    }
}

void reply(Res *res, int status, const void *body, size_t body_len)
{
    if (!res)
        return;

    res->replied = true;

    if (!res->client_socket)
    {
        if (res->arena)
            arena_reset(res->arena);
        return;
    }

    if (uv_is_closing((uv_handle_t *)res->client_socket) ||
        !uv_is_readable((uv_stream_t *)res->client_socket) ||
        !uv_is_writable((uv_stream_t *)res->client_socket))
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    if (!body)
        body_len = 0;

    size_t original_body_len = body_len;
    if (res->is_head_request)
    {
        body = NULL;
        body_len = 0;
    }

    const char *date_str = get_cached_date();
    const char *connection = res->keep_alive ? "keep-alive" : "close";

    size_t headers_size = 0;
    for (uint16_t i = 0; i < res->header_count; i++)
    {
        if (res->headers[i].name && res->headers[i].value)
        {
            headers_size += strlen(res->headers[i].name) + 2 +  // "name: "
                           strlen(res->headers[i].value) + 2;   // "value\r\n"
        }
    }

    char *all_headers = NULL;
    if (headers_size > 0)
    {
        all_headers = arena_alloc(res->arena, headers_size + 1);
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
                size_t name_len = strlen(res->headers[i].name);
                size_t value_len = strlen(res->headers[i].value);
                
                memcpy(all_headers + pos, res->headers[i].name, name_len);
                pos += name_len;
                all_headers[pos++] = ':';
                all_headers[pos++] = ' ';
                memcpy(all_headers + pos, res->headers[i].value, value_len);
                pos += value_len;
                all_headers[pos++] = '\r';
                all_headers[pos++] = '\n';
            }
        }
        all_headers[pos] = '\0';
    }
    else
    {
        all_headers = arena_strdup(res->arena, "");
        if (!all_headers)
        {
            send_error(res->arena, res->client_socket, 500);
            return;
        }
    }

    // Build HTTP response
    char *headers = arena_sprintf(res->arena,
        "HTTP/1.1 %d\r\n"
        "Date: %s\r\n"
        "%s"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "\r\n",
        status,
        date_str,
        all_headers,
        original_body_len,
        connection);

    if (!headers)
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    size_t headers_len = strlen(headers);
    size_t total_len = headers_len + body_len;

    char *response = arena_alloc(res->arena, total_len);
    if (!response)
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    memcpy(response, headers, headers_len);
    if (body_len > 0 && body)
        memcpy(response + headers_len, body, body_len);

    write_req_t *write_req = arena_alloc(res->arena, sizeof(write_req_t));
    if (!write_req)
    {
        send_error(res->arena, res->client_socket, 500);
        return;
    }

    memset(write_req, 0, sizeof(write_req_t));
    write_req->data = response;
    write_req->arena = res->arena;
    write_req->client = (client_t *)res->client_socket->data;
    write_req->buf = uv_buf_init(response, (unsigned int)total_len);

    if (uv_is_closing((uv_handle_t *)res->client_socket))
    {
        arena_reset(res->arena);
        return;
    }

    int result = uv_write(&write_req->req, (uv_stream_t *)res->client_socket,
                          &write_req->buf, 1, write_completion_cb);

    if (result < 0)
    {
        LOG_DEBUG("Write error: %s", uv_strerror(result));
        arena_reset(res->arena);

        if (write_req->client)
            end_request(write_req->client);
    }
}

static bool is_valid_header_char(char c)
{
    unsigned char uc = (unsigned char)c;
    
    if (uc == '\r' || uc == '\n')
        return false;
    
    return (uc == '\t') || (uc >= 32 && uc <= 126);
}

static bool is_valid_header_name(const char *name)
{
    if (!name || !*name)
        return false;

    for (const char *p = name; *p; p++)
    {
        unsigned char c = *p;
        if (!(isalnum(c) || c == '-' || c == '_'))
            return false;
    }

    return true;
}

static bool is_valid_header_value(const char *value)
{
    if (!value)
        return false;

    for (const char *p = value; *p; p++)
    {
        if (*p == '\r' || *p == '\n')
        {
            LOG_ERROR("Invalid character in header value: CRLF detected");
            return false;
        }

        if (!is_valid_header_char(*p))
        {
            LOG_ERROR("Invalid character in header value: 0x%02x", (unsigned char)*p);
            return false;
        }
    }

    return true;
}

void set_header(Res *res, const char *name, const char *value)
{
    if (!res || !res->arena || !name || !value)
    {
        LOG_ERROR("Invalid argument(s) to set_header");
        return;
    }

    if (!is_valid_header_name(name))
    {
        LOG_ERROR("Invalid header name: '%s'", name);
        return;
    }

    if (!is_valid_header_value(value))
    {
        LOG_ERROR("Invalid header value for '%s'", name);
        return;
    }

#ifdef ECEWO_DEBUG
    // Check for duplicate headers and warn
    // but still add the header, do not override
    for (uint16_t i = 0; i < res->header_count; i++)
    {
        if (res->headers[i].name && 
            strcasecmp(res->headers[i].name, name) == 0)
        {
            LOG_DEBUG("Warning: Duplicate header '%s' detected!", name);
            LOG_DEBUG("  Existing value: '%s'", res->headers[i].value);
            LOG_DEBUG("  New value: '%s'", value);
            LOG_DEBUG("  Both will be sent (this may cause issues)");
            break;
        }
    }
#endif
    
    if (res->header_count >= res->header_capacity)
    {
        uint16_t new_cap = res->header_capacity ? res->header_capacity * 2 : 8;
        
        http_header_t *tmp = arena_realloc(res->arena, res->headers,
                                           res->header_capacity * sizeof(http_header_t),
                                           new_cap * sizeof(http_header_t));

        if (!tmp)
        {
            LOG_ERROR("Failed to realloc headers array");
            return;
        }

        memset(&tmp[res->header_capacity], 0, (new_cap - res->header_capacity) * sizeof(http_header_t));

        res->headers = tmp;
        res->header_capacity = new_cap;
    }

    res->headers[res->header_count].name = arena_strdup(res->arena, name);
    res->headers[res->header_count].value = arena_strdup(res->arena, value);

    if (!res->headers[res->header_count].name || !res->headers[res->header_count].value)
    {
        LOG_ERROR("Failed to allocate memory in set_header");
        return;
    }

    res->header_count++;
}

void redirect(Res *res, int status, const char *url)
{
    if (!res || !url)
        return;
    
    if (!is_valid_header_value(url))
    {
        LOG_ERROR("Invalid redirect URL (CRLF detected)");
        send_text(res, BAD_REQUEST, "Bad Request");
        return;
    }
    
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

    set_header(res, "Content-Type", "text/plain");
    reply(res, status, message, message_len);
}
