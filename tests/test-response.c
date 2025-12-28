#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"

// JSON
void handler_json_response(Req *req, Res *res)
{
    (void)req;
    send_json(res, 200, "{\"status\":\"ok\"}");
}

int test_json_content_type(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/json-response"
    };

    MockResponse res = request(&params);

    ASSERT_EQ(200, res.status_code);
    ASSERT_EQ_STR("{\"status\":\"ok\"}", res.body);

    free_request(&res);
    RETURN_OK();
}

// HTML
void handler_html_response(Req *req, Res *res)
{
    (void)req;
    send_html(res, 200, "<h1>Hello</h1>");
}

int test_html_content_type(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/html-response"
    };

    MockResponse res = request(&params);

    ASSERT_EQ(200, res.status_code);
    ASSERT_EQ_STR("<h1>Hello</h1>", res.body);

    free_request(&res);
    RETURN_OK();
}

// STATUS CODES
void handler_status_codes(Req *req, Res *res)
{
    const char *code = get_query(req, "code");
    if (!code) {
        send_text(res, 400, "Missing code");
        return;
    }

    int status = atoi(code);
    send_text(res, status, "Status test");
}

int test_status_201(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/status?code=201"
    };

    MockResponse res = request(&params);
    ASSERT_EQ(201, res.status_code);

    free_request(&res);
    RETURN_OK();
}

int test_status_404(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/status?code=404"
    };

    MockResponse res = request(&params);
    ASSERT_EQ(404, res.status_code);

    free_request(&res);
    RETURN_OK();
}

int test_status_500(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/status?code=500"
    };

    MockResponse res = request(&params);
    ASSERT_EQ(500, res.status_code);

    free_request(&res);
    RETURN_OK();
}

int test_404_unknown_path(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/this/path/does/not/exist"
    };

    MockResponse res = request(&params);
    ASSERT_EQ(404, res.status_code);

    free_request(&res);
    RETURN_OK();
}

int test_404_wrong_method(void)
{
    // /users/:id regsitered as GET only
    MockParams params = {
        .method = MOCK_DELETE,
        .path = "/users/123"
    };

    MockResponse res = request(&params);
    ASSERT_EQ(404, res.status_code);

    free_request(&res);
    RETURN_OK();
}
