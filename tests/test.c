#include "unity.h"
#include "ecewo.h"
#include "mock.h"
#include "uv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#define MAX_RETRIES 50
#define RETRY_DELAY_MS 100

static uv_thread_t server_thread;
static volatile bool server_ready = false;
static volatile bool shutdown_requested = false;

void hello_handler(Req *req, Res *res)
{
    send_text(res, 200, "Hello, World!");
}

void echo_handler(Req *req, Res *res)
{
    if (!req->body || req->body_len == 0)
    {
        send_text(res, 400, "No body");
        return;
    }
    send_text(res, 200, req->body);
}

void json_handler(Req *req, Res *res)
{
    send_json(res, 200, "{\"status\":\"ok\",\"message\":\"JSON response\"}");
}

void html_handler(Req *req, Res *res)
{
    send_html(res, 200, "<html><body>Test</body></html>");
}

void multi_param_handler(Req *req, Res *res)
{
    const char *user_id = get_param(req, "userId");
    const char *post_id = get_param(req, "postId");

    char *response = ecewo_sprintf(res, "{\"userId\": %s, \"postId\": %s}", user_id, post_id);
    send_json(res, 200, response);
}

void query_handler(Req *req, Res *res)
{
    const char *name = get_query(req, "name");
    const char *age = get_query(req, "age");

    if (!name || !age)
    {
        send_json(res, 400, "{\"error\":\"Missing parameters\"}");
        return;
    }

    char *json = ecewo_sprintf(res, "{\"name\":\"%s\",\"age\":%s}", name, age);
    send_json(res, 200, json);
}

int auth_middleware(Req *req, Res *res, Chain *chain)
{
    const char *auth = get_header(req, "Authorization");
    if (!auth || strcmp(auth, "Bearer secret") != 0)
    {
        send_json(res, 401, "{\"error\":\"Unauthorized\"}");
        return 0;
    }
    return next(req, res, chain);
}

void protected_handler(Req *req, Res *res)
{
    send_json(res, 200, "{\"data\":\"protected resource\"}");
}

void shutdown_handler(Req *req, Res *res)
{
    send_text(res, 200, "Shutting down");
    uv_stop(get_loop());
}

void server_thread_func(void *arg)
{
    (void)arg;

    if (server_init() != SERVER_OK)
    {
        fprintf(stderr, "Failed to initialize server\n");
        return;
    }

    get("/", hello_handler);
    post("/echo", echo_handler);
    get("/api/json", json_handler);
    get("/api/html", html_handler);
    get("/api/users/:userId/posts/:postId", multi_param_handler);
    get("/api/search", query_handler);
    get("/protected", use(auth_middleware), protected_handler);
    get("/shutdown", shutdown_handler);

    if (server_listen(TEST_PORT) != SERVER_OK)
    {
        fprintf(stderr, "Failed to start server on port %d\n", TEST_PORT);
        return;
    }

    server_ready = true;
    printf("Server ready on port %d\n", TEST_PORT);

    server_run();
}

bool wait_for_server_ready(void)
{
    for (int i = 0; i < MAX_RETRIES; i++)
    {
        if (server_ready)
        {
            http_response_t resp = http_request("GET", "/", NULL, NULL);
            if (resp.status_code == 200)
            {
                free_response(&resp);
                return true;
            }
            free_response(&resp);
        }

        uv_sleep(RETRY_DELAY_MS);
    }

    return false;
}

// ============================================================================
// TEST SETUP & TEARDOWN
// ============================================================================

void setUp(void)
{
    // Individual tests don't need per-test setup
}

void tearDown(void)
{
    // Individual tests don't need per-test teardown
}

void suiteSetUp(void)
{
    printf("\n=== Starting Test Suite ===\n");
    printf("Initializing server on port %d...\n", TEST_PORT);

    server_ready = false;
    shutdown_requested = false;

    int result = uv_thread_create(&server_thread, server_thread_func, NULL);
    if (result != 0)
    {
        fprintf(stderr, "Failed to create server thread: %s\n", uv_strerror(result));
        return;
    }

    printf("Waiting for server to be ready...\n");
    if (!wait_for_server_ready())
    {
        fprintf(stderr, "Server failed to start within timeout!\n");
        return;
    }

    printf("Server is ready!\n\n");
}

int suiteTearDown(int num_failures)
{
    printf("\n=== Cleaning Up Test Suite ===\n");
    
    printf("Sending shutdown request...\n");
    http_response_t resp = http_request("GET", "/shutdown", NULL, NULL);
    free_response(&resp);
    
    printf("Waiting for server thread to finish...\n");
    uv_thread_join(&server_thread);
    
    printf("Cleanup complete\n\n");
    
    return num_failures;
}

// ============================================================================
// TESTS
// ============================================================================

void test_simple_get_request(void)
{
    http_response_t resp = http_request("GET", "/", NULL, NULL);

    TEST_ASSERT_EQUAL(200, resp.status_code);
    TEST_ASSERT_NOT_NULL(resp.body);
    TEST_ASSERT_EQUAL_STRING("Hello, World!", resp.body);

    free_response(&resp);
}

void test_post_with_body(void)
{
    const char *body = "Echo this text";
    http_response_t resp = http_request("POST", "/echo", body, NULL);

    TEST_ASSERT_EQUAL(200, resp.status_code);
    TEST_ASSERT_NOT_NULL(resp.body);
    TEST_ASSERT_EQUAL_STRING(body, resp.body);

    free_response(&resp);
}

void test_json_response(void)
{
    http_response_t resp = http_request("GET", "/api/json", NULL, NULL);

    TEST_ASSERT_EQUAL(200, resp.status_code);
    TEST_ASSERT_NOT_NULL(resp.body);
    TEST_ASSERT_NOT_NULL(strstr(resp.body, "\"status\":\"ok\""));
    TEST_ASSERT_NOT_NULL(strstr(resp.body, "\"message\":\"JSON response\""));

    free_response(&resp);
}

void test_html_response(void)
{
    http_response_t resp = http_request("GET", "/api/html", NULL, NULL);

    TEST_ASSERT_EQUAL(200, resp.status_code);
    TEST_ASSERT_NOT_NULL(resp.body);
    TEST_ASSERT_EQUAL_STRING(resp.body, "<html><body>Test</body></html>");

    free_response(&resp);
}

void test_url_parameters(void)
{
    http_response_t resp = http_request("GET", "/api/users/123/posts/456", NULL, NULL);

    TEST_ASSERT_EQUAL(200, resp.status_code);
    TEST_ASSERT_NOT_NULL(resp.body);
    TEST_ASSERT_NOT_NULL(strstr(resp.body, "\"userId\": 123"));
    TEST_ASSERT_NOT_NULL(strstr(resp.body, "\"postId\": 456"));

    free_response(&resp);
}

void test_query_parameters(void)
{
    http_response_t resp = http_request("GET", "/api/search?name=John&age=30", NULL, NULL);

    TEST_ASSERT_EQUAL(200, resp.status_code);
    TEST_ASSERT_NOT_NULL(resp.body);
    TEST_ASSERT_NOT_NULL(strstr(resp.body, "\"name\":\"John\""));
    TEST_ASSERT_NOT_NULL(strstr(resp.body, "\"age\":30"));

    free_response(&resp);
}

void test_middleware_auth_success(void)
{
    http_response_t resp = http_request("GET", "/protected", NULL,
                                        "Authorization: Bearer secret\r\n");

    TEST_ASSERT_EQUAL(200, resp.status_code);
    TEST_ASSERT_NOT_NULL(resp.body);
    TEST_ASSERT_NOT_NULL(strstr(resp.body, "\"data\":\"protected resource\""));

    free_response(&resp);
}

void test_middleware_auth_failure(void)
{
    http_response_t resp = http_request("GET", "/protected", NULL, NULL);

    TEST_ASSERT_EQUAL(401, resp.status_code);
    TEST_ASSERT_NOT_NULL(resp.body);
    TEST_ASSERT_NOT_NULL(strstr(resp.body, "\"error\":\"Unauthorized\""));

    free_response(&resp);
}

void test_404_not_found(void)
{
    http_response_t resp = http_request("GET", "/nonexistent", NULL, NULL);

    TEST_ASSERT_EQUAL(404, resp.status_code);

    free_response(&resp);
}

void test_multiple_sequential_requests(void)
{
    for (int i = 0; i < 5; i++)
    {
        http_response_t resp = http_request("GET", "/", NULL, NULL);
        TEST_ASSERT_EQUAL(200, resp.status_code);
        free_response(&resp);
    }
}

void test_concurrent_requests(void)
{
    // Simple concurrency test - make requests back to back
    http_response_t responses[3];

    for (int i = 0; i < 3; i++)
    {
        responses[i] = http_request("GET", "/", NULL, NULL);
    }

    for (int i = 0; i < 3; i++)
    {
        TEST_ASSERT_EQUAL(200, responses[i].status_code);
        free_response(&responses[i]);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(void)
{
    suiteSetUp();
    UNITY_BEGIN();

    RUN_TEST(test_simple_get_request);
    RUN_TEST(test_post_with_body);
    RUN_TEST(test_json_response);
    RUN_TEST(test_html_response);
    RUN_TEST(test_url_parameters);
    RUN_TEST(test_query_parameters);
    RUN_TEST(test_middleware_auth_success);
    RUN_TEST(test_middleware_auth_failure);
    RUN_TEST(test_404_not_found);
    RUN_TEST(test_multiple_sequential_requests);
    RUN_TEST(test_concurrent_requests);

    int result = UNITY_END();

    suiteTearDown(result);
    return result;
}
