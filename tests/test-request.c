#include "ecewo.h"
#include "mock.h"
#include "unity.h"
#include <string.h>

void handler_params(Req *req, Res *res)
{
    const char *user_id = get_param(req, "userId");
    const char *post_id = get_param(req, "postId");

    char *response = ecewo_sprintf(res, "{\"userId\": %s, \"postId\": %s}", user_id, post_id);
    send_json(res, 200, response);
}

void test_params(void)
{
    MockParams params = {
        .method = GET,
        .path = "/users/123/posts/456"
    };

    MockResponse res = request(&params);

    TEST_ASSERT_EQUAL(200, res.status_code);
    TEST_ASSERT_NOT_NULL(res.body);
    TEST_ASSERT_EQUAL_STRING("{\"userId\": 123, \"postId\": 456}", res.body);

    free_request(&res);
}

void handler_query(Req *req, Res *res)
{
    const char *name = get_query(req, "name");
    const char *age = get_query(req, "age");

    if (!name || !age)
    {
        send_text(res, 400, "Error: Missing parameters.");
        return;
    }

    char *json = ecewo_sprintf(res, "Name: %s, Age: %s", name, age);
    send_text(res, 200, json);
}

void test_query(void)
{
    MockParams params = {
        .method = GET,
        .path = "/search?name=John&age=30"
    };

    MockResponse res = request(&params);

    TEST_ASSERT_EQUAL(200, res.status_code);
    TEST_ASSERT_NOT_NULL(res.body);
    TEST_ASSERT_EQUAL_STRING("Name: John, Age: 30", res.body);

    free_request(&res);
}

void handler_headers(Req *req, Res *res)
{
    const char *authorization = get_header(req, "Authorization");
    const char *content_type = get_header(req, "Content-Type");
    const char *x_custom_header = get_header(req, "X-Custom-Header");

    if (!authorization || !content_type || !x_custom_header)
    {
        send_text(res, BAD_REQUEST, "Missing required headers");
        return;
    }

    if (!req->body || req->body_len == 0)
    {
        send_text(res, BAD_REQUEST, "Empty body");
        return;
    }

    const char *expected_body = "{\"name\":\"John\",\"age\":30}";
    if (strcmp(req->body, expected_body) != 0)
    {
        char *error = ecewo_sprintf(res, 
            "Body mismatch. Expected: %s, Got: %s", 
            expected_body, req->body);
        send_text(res, BAD_REQUEST, error);
        return;
    }

    send_text(res, CREATED, "Success!");
}

void test_headers(void)
{
    MockHeaders headers[] = {
        {"Authorization", "Bearer secret-token"},
        {"Content-Type", "application/json"},
        {"X-Custom-Header", "custom-value"}
    };
    
    const char *json_body = "{\"name\":\"John\",\"age\":30}";
    
    MockParams params = {
        .method = POST,
        .path = "/headers",
        .body = json_body,
        .headers = headers,
        .header_count = 3
    };

    MockResponse res = request(&params);

    TEST_ASSERT_EQUAL(201, res.status_code);
    TEST_ASSERT_EQUAL_STRING("Success!", res.body);

    free_request(&res);
}
