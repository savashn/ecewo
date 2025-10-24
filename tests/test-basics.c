#include "ecewo.h"
#include "ecewo/mock.h"
#include "unity.h"
#include <string.h>

void handler_plaintext(Req *req, Res *res)
{
    send_text(res, 200, "Hello, World!");
}

void test_plaintext(void)
{
    MockParams params = {
        .method = GET,
        .path = "/plaintext",
    };

    MockResponse res = request(&params);

    TEST_ASSERT_EQUAL(200, res.status_code);
    TEST_ASSERT_NOT_NULL(res.body);
    TEST_ASSERT_EQUAL_STRING("Hello, World!", res.body);

    free_request(&res);
}

void handler_json(Req *req, Res *res)
{
    send_json(res, 200, "{\"Hello\":\"World!\"}");
}

void test_json(void)
{
    MockParams params = {
        .method = GET,
        .path = "/json",
    };

    MockResponse res = request(&params);

    TEST_ASSERT_EQUAL(200, res.status_code);
    TEST_ASSERT_NOT_NULL(res.body);
    TEST_ASSERT_EQUAL_STRING("{\"Hello\":\"World!\"}", res.body);

    free_request(&res);
}

void handler_html(Req *req, Res *res)
{
    send_html(res, 200, "<html><body>Test</body></html>");
}

void test_html(void)
{
    MockParams params = {
        .method = GET,
        .path = "/html",
    };

    MockResponse res = request(&params);

    TEST_ASSERT_EQUAL(200, res.status_code);
    TEST_ASSERT_NOT_NULL(res.body);
    TEST_ASSERT_EQUAL_STRING("<html><body>Test</body></html>", res.body);

    free_request(&res);
}
