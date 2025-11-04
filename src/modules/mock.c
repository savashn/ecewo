#include "ecewo.h"
#include "ecewo/mock.h"
#include "uv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#define MAX_RETRIES 10
#define RETRY_DELAY_MS 100

static uv_thread_t server_thread;
static bool server_ready = false;
static bool shutdown_requested = false;
static test_routes_cb_t test_routes = NULL;

void free_request(MockResponse *res)
{
    if (res && res->body)
    {
        free(res->body);
        res->body = NULL;
    }
}

MockResponse request(MockParams *params)
{
    MockResponse response = {0};

#ifdef _WIN32
    static bool wsa_initialized = false;
    if (!wsa_initialized)
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            response.status_code = -1;
            return response;
        }
        wsa_initialized = true;
    }
#endif

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        response.status_code = -1;
        return response;
    }

#ifdef _WIN32
    DWORD timeout_ms = 5000;  // 5 seconds
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));
#else
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
#endif

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TEST_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        response.status_code = -1;
        return response;
    }

    // Build HTTP request
    char request[8192];
    int len = 0;
    const char *method = "GET";

    switch (params->method)
    {
        case MOCK_GET:
            method = "GET";
            break;
        case MOCK_POST:
            method = "POST";
            break;
        case MOCK_PUT:
            method = "PUT";
            break;
        case MOCK_PATCH:
            method = "PATCH";
            break;
        case MOCK_DELETE:
            method = "DELETE";
            break;
    }

    len += snprintf(request + len, sizeof(request) - len,
                    "%s %s HTTP/1.1\r\n"
                    "Host: localhost:%d\r\n",
                    method, params->path, TEST_PORT);

    if (params->headers && params->header_count > 0)
    {
        for (size_t i = 0; i < params->header_count; i++)
        {
            len += snprintf(request + len, sizeof(request) - len,
                           "%s: %s\r\n",
                           params->headers[i].key,
                           params->headers[i].value);
        }
    }

    if (params->body && strlen(params->body) > 0)
    {
        len += snprintf(request + len, sizeof(request) - len,
                       "Content-Length: %zu\r\n",
                       strlen(params->body));
    }

    len += snprintf(request + len, sizeof(request) - len, "\r\n");

    if (params->body && strlen(params->body) > 0)
    {
        len += snprintf(request + len, sizeof(request) - len,
                       "%s", params->body);
    }

    if (send(sock, request, len, 0) < 0)
    {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        response.status_code = -1;
        return response;
    }

    char buffer[8192] = {0};
    int total_received = 0;
    int received;

    while (total_received < (int)sizeof(buffer) - 1)
    {
        received = recv(sock, buffer + total_received,
                       sizeof(buffer) - total_received - 1, 0);
        if (received <= 0)
            break;
        total_received += received;
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif

    if (total_received <= 0)
    {
        response.status_code = -1;
        return response;
    }

    if (sscanf(buffer, "HTTP/1.1 %hu", &response.status_code) != 1)
    {
        response.status_code = -1;
        return response;
    }

    char *body_start = strstr(buffer, "\r\n\r\n");
    if (body_start)
    {
        body_start += 4;
        response.body_len = strlen(body_start);
        response.body = malloc(response.body_len + 1);
        if (response.body)
        {
            strcpy(response.body, body_start);
        }
    }

    return response;
}

void test_routes_hook(test_routes_cb_t callback)
{
    test_routes = callback;
}

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
        fprintf(stderr, "Failed to initialize server\n");
        return;
    }

    if (test_routes)
        test_routes();

    get("/ecewo-test-shutdown", shutdown_handler);
    get("/ecewo-test-check", test_handler);

    if (server_listen(TEST_PORT) != SERVER_OK)
    {
        fprintf(stderr, "Failed to start server on port %d\n", TEST_PORT);
        return;
    }

    server_ready = true;
    printf("Server ready on port %d\n", TEST_PORT);

    server_run();
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

int ecewo_test_setup(void)
{
    printf("\n=== Starting Test Suite ===\n");

    server_ready = false;
    shutdown_requested = false;

    int result = uv_thread_create(&server_thread, server_thread_fn, NULL);
    if (result != 0)
    {
        fprintf(stderr, "Failed to create server thread: %s\n", uv_strerror(result));
        return -1;
    }

    if (!wait_for_server_ready())
    {
        fprintf(stderr, "Server failed to start within timeout!\n");
        return -1;
    }

    return 0;
}

int ecewo_test_tear_down(int num_failures)
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
    
    return num_failures;
}
