#include "ecewo.h"
#include "ecewo/mock.h"
#include "tester.h"
#include <string.h>

// ========================================
// PLAINTEXT
// ========================================

void handler_plaintext(Req *req, Res *res)
{
    send_text(res, 200, "Hello, World!");
}

int test_plaintext(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/plaintext",
    };

    MockResponse res = request(&params);

    ASSERT_EQ(200, res.status_code);
    ASSERT_NOT_NULL(res.body);
    ASSERT_EQ_STR("Hello, World!", res.body);

    free_request(&res);

    RETURN_OK();
}


// ===========================================
// JSON
// ===========================================

void handler_json(Req *req, Res *res)
{
    send_json(res, 200, "{\"Hello\":\"World!\"}");
}

int test_json(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/json",
    };

    MockResponse res = request(&params);

    ASSERT_EQ(200, res.status_code);
    ASSERT_NOT_NULL(res.body);
    ASSERT_EQ_STR("{\"Hello\":\"World!\"}", res.body);

    free_request(&res);

    RETURN_OK();
}


// ===========================================
// HTML
// ===========================================

void handler_html(Req *req, Res *res)
{
    send_html(res, 200, "<html><body>Test</body></html>");
}

int test_html(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/html",
    };

    MockResponse res = request(&params);

    ASSERT_EQ(200, res.status_code);
    ASSERT_NOT_NULL(res.body);
    ASSERT_EQ_STR("<html><body>Test</body></html>", res.body);

    free_request(&res);

    RETURN_OK();
}
