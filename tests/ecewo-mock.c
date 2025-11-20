#include "ecewo.h"
#include "ecewo-mock.h"
#include "uv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_RETRIES 10
#define RETRY_DELAY_MS 100
#define BUFFER_SIZE 8192

static uv_thread_t server_thread;
static bool server_ready = false;
static bool shutdown_requested = false;
static test_routes_cb_t test_routes = NULL;

typedef struct {
    uv_tcp_t tcp;
    uv_connect_t connect_req;
    uv_write_t write_req;
    uv_shutdown_t shutdown_req;
    MockResponse *response;
    char *request_data;
    char *response_buffer;
    size_t response_len;
    size_t response_capacity;
    bool done;
    int status;
    uv_loop_t *loop;
} http_client_t;

static void shutdown_handler(Req *req, Res *res)
{
    send_text(res, 200, "Shutting down");
    uv_stop(get_loop());
}

static void test_handler(Req *req, Res *res)
{
    send_text(res, OK, "Test");
}

static void server_thread_fn(void *arg)
{
    (void)arg;

    if (server_init() != SERVER_OK)
    {
        LOG_ERROR("Failed to initialize server");
        return;
    }

    if (test_routes)
        test_routes();

    get("/ecewo-test-shutdown", shutdown_handler);
    get("/ecewo-test-check", test_handler);

    if (server_listen(TEST_PORT) != SERVER_OK)
    {
        LOG_ERROR("Failed to start server on port %d", TEST_PORT);
        return;
    }

    server_ready = true;
    server_run();
}

static void on_close(uv_handle_t *handle)
{
    http_client_t *client = (http_client_t *)handle->data;
    LOG_DEBUG("on_close called - connection fully closed");
    client->done = true;
}

static void on_shutdown(uv_shutdown_t *req, int status)
{
    http_client_t *client = (http_client_t *)req->data;
    LOG_DEBUG("on_shutdown called, status=%d", status);
    uv_close((uv_handle_t *)&client->tcp, on_close);
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    http_client_t *client = (http_client_t *)handle->data;
    
    // Expand buffer if needed
    if (client->response_len + suggested_size > client->response_capacity)
    {
        size_t new_capacity = client->response_capacity * 2;
        if (new_capacity < client->response_len + suggested_size)
            new_capacity = client->response_len + suggested_size;
        
        char *new_buffer = realloc(client->response_buffer, new_capacity);
        if (new_buffer)
        {
            client->response_buffer = new_buffer;
            client->response_capacity = new_capacity;
        }
    }
    
    buf->base = client->response_buffer + client->response_len;
    buf->len = client->response_capacity - client->response_len;
}

static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    http_client_t *client = (http_client_t *)stream->data;
    LOG_DEBUG("on_read called, nread=%zd", nread);
    
    if (nread < 0)
    {
        if (nread != UV_EOF)
        {
            LOG_ERROR("Read error: %s", uv_strerror((int)nread));
            client->status = -1;
        }
        else
        {
            LOG_DEBUG("Received EOF, connection closed by server");
        }
        
        uv_shutdown(&client->shutdown_req, stream, on_shutdown);
        client->shutdown_req.data = client;
        return;
    }
    
    if (nread == 0)
    {
        LOG_DEBUG("Read 0 bytes (would block)");
        return;
    }
    
    client->response_len += nread;
    client->response_buffer[client->response_len] = '\0';
    
    LOG_DEBUG("Read %zd bytes, total: %zu\n", nread, client->response_len);
    LOG_DEBUG("Current buffer content: %.100s%s\n", 
           client->response_buffer, client->response_len > 100 ? "..." : "");
    
    // Check if we have complete HTTP response
    if (strstr(client->response_buffer, "\r\n\r\n"))
    {
        LOG_DEBUG("Complete HTTP response detected (found \\r\\n\\r\\n)\n");
        uv_read_stop(stream);
        uv_shutdown(&client->shutdown_req, stream, on_shutdown);
        client->shutdown_req.data = client;
    }
    else
    {
        LOG_DEBUG("Incomplete response, waiting for more data");
    }
}

static void on_write(uv_write_t *req, int status)
{
    http_client_t *client = (http_client_t *)req->data;
    LOG_DEBUG("on_write called, status=%d (%s)\n", status, 
           status < 0 ? uv_strerror(status) : "OK");
    
    if (status < 0)
    {
        LOG_ERROR("Write error: %s", uv_strerror(status));
        client->status = -1;
        uv_close((uv_handle_t *)&client->tcp, on_close);
        return;
    }
    
    LOG_DEBUG("Starting to read response");
    int result = uv_read_start((uv_stream_t *)&client->tcp, alloc_buffer, on_read);
    if (result < 0)
    {
        LOG_ERROR("Read start error: %s", uv_strerror(result));
        client->status = -1;
        uv_close((uv_handle_t *)&client->tcp, on_close);
    }
}

static void on_connect(uv_connect_t *req, int status)
{
    http_client_t *client = (http_client_t *)req->data;
    LOG_DEBUG("on_connect called, status=%d (%s)\n", status, 
           status < 0 ? uv_strerror(status) : "OK");
    
    if (status < 0)
    {
        LOG_ERROR("Connection error: %s", uv_strerror(status));
        client->status = -1;
        uv_close((uv_handle_t *)&client->tcp, on_close);
        return;
    }
    
    uv_buf_t buf = uv_buf_init(client->request_data, strlen(client->request_data));
    client->write_req.data = client;
    
    LOG_DEBUG("Sending request (%zu bytes):\n%.200s\n", 
           strlen(client->request_data), client->request_data);
    
    int result = uv_write(&client->write_req, (uv_stream_t *)&client->tcp, &buf, 1, on_write);
    if (result < 0)
    {
        LOG_ERROR("Write error: %s", uv_strerror(result));
        client->status = -1;
        uv_close((uv_handle_t *)&client->tcp, on_close);
    }
}

static char *build_http_request(MockParams *params)
{
    char *request = malloc(BUFFER_SIZE);
    if (!request) return NULL;
    
    int len = 0;
    const char *method = "GET";

    switch (params->method)
    {
        case MOCK_GET:    method = "GET"; break;
        case MOCK_POST:   method = "POST"; break;
        case MOCK_PUT:    method = "PUT"; break;
        case MOCK_PATCH:  method = "PATCH"; break;
        case MOCK_DELETE: method = "DELETE"; break;
    }

    len += snprintf(request + len, BUFFER_SIZE - len,
                    "%s %s HTTP/1.1\r\n"
                    "Host: localhost:%d\r\n"
                    "Connection: close\r\n",
                    method, params->path, TEST_PORT);

    if (params->headers && params->header_count > 0)
    {
        for (size_t i = 0; i < params->header_count; i++)
        {
            len += snprintf(request + len, BUFFER_SIZE - len,
                           "%s: %s\r\n",
                           params->headers[i].key,
                           params->headers[i].value);
        }
    }

    if (params->body && strlen(params->body) > 0)
    {
        len += snprintf(request + len, BUFFER_SIZE - len,
                       "Content-Length: %zu\r\n",
                       strlen(params->body));
    }

    len += snprintf(request + len, BUFFER_SIZE - len, "\r\n");

    if (params->body && strlen(params->body) > 0)
    {
        len += snprintf(request + len, BUFFER_SIZE - len, "%s", params->body);
    }

    return request;
}

static void parse_response(http_client_t *client)
{
    LOG_DEBUG("parse_response called");
    LOG_DEBUG("Response buffer length: %zu\n", client->response_len);
    
    if (!client->response_buffer || client->response_len == 0)
    {
        LOG_DEBUG("No response buffer or empty response");
        client->response->status_code = -1;
        return;
    }

    LOG_DEBUG("Response buffer content:\n--- START ---\n%.*s\n--- END ---\n", 
           (int)client->response_len, client->response_buffer);

    unsigned short status;
    if (sscanf(client->response_buffer, "HTTP/1.1 %hu", &status) != 1)
    {
        LOG_DEBUG("Failed to parse HTTP status line");
        client->response->status_code = -1;
        return;
    }
    
    client->response->status_code = status;
    LOG_DEBUG("Parsed status code: %hu\n", status);

    char *body_start = strstr(client->response_buffer, "\r\n\r\n");
    if (body_start)
    {
        body_start += 4;
        size_t body_len = strlen(body_start);
        LOG_DEBUG("Found body separator, body length: %zu\n", body_len);
        LOG_DEBUG("Body content: '%s'\n", body_start);
        
        if (body_len > 0)
        {
            client->response->body = malloc(body_len + 1);
            if (client->response->body)
            {
                strcpy(client->response->body, body_start);
                client->response->body_len = body_len;
                LOG_DEBUG("Body copied successfully");
            }
            else
            {
                LOG_DEBUG("Failed to allocate memory for body");
            }
        }
        else
        {
            LOG_DEBUG("Empty body");
        }
    }
    else
    {
        LOG_DEBUG("No body separator found in response");
    }
}

static bool wait_for_server_ready(void)
{
    for (int i = 0; i < MAX_RETRIES; i++)
    {
        if (server_ready)
        {
            MockParams params = {
                .method = MOCK_GET,
                .path = "/ecewo-test-check",
                .body = NULL,
                .headers = NULL,
                .header_count = 0
            };
            
            MockResponse resp = request(&params);
            if (resp.status_code == 200)
            {
                free_request(&resp);
                return true;
            }
            free_request(&resp);
        }

        uv_sleep(RETRY_DELAY_MS);
    }

    return false;
}

void free_request(MockResponse *res)
{
    if (res && res->body)
    {
        free(res->body);
        res->body = NULL;
        res->body_len = 0;
    }
}

MockResponse request(MockParams *params)
{
    uint64_t start_time = uv_hrtime();
    
    MockResponse response = {0};
    
    char *request_data = build_http_request(params);
    if (!request_data)
    {
        response.status_code = -1;
        return response;
    }

    uv_loop_t loop;
    int result = uv_loop_init(&loop);
    if (result < 0)
    {
        LOG_ERROR("Failed to initialize event loop: %s\n", uv_strerror(result));
        free(request_data);
        response.status_code = -1;
        return response;
    }

    http_client_t client = {0};
    client.loop = &loop;
    client.response = &response;
    client.request_data = request_data;
    client.response_capacity = BUFFER_SIZE;
    client.response_buffer = malloc(client.response_capacity);
    client.done = false;
    client.status = 0;

    if (!client.response_buffer)
    {
        LOG_ERROR("Failed to allocate response buffer");
        free(request_data);
        uv_loop_close(&loop);
        response.status_code = -1;
        return response;
    }

    result = uv_tcp_init(&loop, &client.tcp);
    if (result < 0)
    {
        LOG_ERROR("Failed to initialize TCP handle: %s\n", uv_strerror(result));
        free(request_data);
        free(client.response_buffer);
        uv_loop_close(&loop);
        response.status_code = -1;
        return response;
    }

    client.tcp.data = &client;
    client.connect_req.data = &client;

    struct sockaddr_in addr;
    result = uv_ip4_addr("127.0.0.1", TEST_PORT, &addr);
    if (result < 0)
    {
        LOG_ERROR("Failed to create address: %s\n", uv_strerror(result));
        free(request_data);
        free(client.response_buffer);
        uv_close((uv_handle_t *)&client.tcp, NULL);
        uv_run(&loop, UV_RUN_DEFAULT);
        uv_loop_close(&loop);
        response.status_code = -1;
        return response;
    }

    result = uv_tcp_connect(&client.connect_req,
                            &client.tcp,
                            (const struct sockaddr *)&addr,
                            on_connect);

    if (result < 0)
    {
        LOG_ERROR("Failed to initiate connection: %s\n", uv_strerror(result));
        free(request_data);
        free(client.response_buffer);
        uv_close((uv_handle_t *)&client.tcp, NULL);
        uv_run(&loop, UV_RUN_DEFAULT);
        uv_loop_close(&loop);
        response.status_code = -1;
        return response;
    }

    uint64_t loop_start = uv_now(&loop);
    int loop_iterations = 0;
    int no_events_count = 0;
    
    while (!client.done && client.status == 0) {
        int events = uv_run(&loop, UV_RUN_ONCE);
        loop_iterations++;
        
        if (events == 0) {
            no_events_count++;
            
            // If we have no events for too many iterations, something might be wrong
            if (no_events_count >= 10) {
                LOG_DEBUG("Too many iterations with no events, breaking");
                client.status = -1;
                break;
            }
            
            // Small delay when no events to prevent busy waiting
            uv_sleep(1);
        } else {
            no_events_count = 0; // Reset counter when we have events
        }
        
        // Check for timeout (5 seconds)
        if ((uv_now(&loop) - loop_start) > 5000) {
            LOG_DEBUG("Request timeout after %d iterations\n", loop_iterations);
            client.status = -1;
            break;
        }
        
        if (loop_iterations % 100 == 0) {
            LOG_DEBUG("Loop iteration %d, done=%d, status=%d\n", 
                   loop_iterations, client.done, client.status);
        }
    }
    
    uint64_t end_time = uv_hrtime();
    uint64_t duration_ms = (end_time - start_time) / 1000000;
    
    if (client.status == 0 && client.response_buffer)
    {
        parse_response(&client);
    }
    else if (client.status != 0)
    {
        LOG_ERROR("Request failed with status %d\n", client.status);
        response.status_code = -1;
    }

    // Cleanup
    free(request_data);
    free(client.response_buffer);
    uv_loop_close(&loop);

    return response;
}

void test_routes_hook(test_routes_cb_t callback)
{
    test_routes = callback;
}

int mock_setup(void)
{
    printf("\n=== Starting Test Suite ===\n");

    server_ready = false;
    shutdown_requested = false;

    int result = uv_thread_create(&server_thread, server_thread_fn, NULL);
    if (result != 0)
    {
        LOG_ERROR("Failed to create server thread: %s", uv_strerror(result));
        return -1;
    }

    if (!wait_for_server_ready())
    {
        LOG_ERROR("Server failed to start within timeout!");
        return -1;
    }

    return 0;
}

void mock_down(void)
{
    printf("\n=== Cleaning Up Test Suite ===\n");
    
    MockParams params = {
        .method = MOCK_GET,
        .path = "/ecewo-test-shutdown",
        .body = NULL,
        .headers = NULL,
        .header_count = 0
    };
    MockResponse resp = request(&params);
    free_request(&resp);
    
    uv_thread_join(&server_thread);
    
    printf("Cleanup complete\n\n");
    
    return;
}
