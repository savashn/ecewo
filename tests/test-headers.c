#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"

void handler_echo_headers(Req *req, Res *res)
{
    const char *auth = get_header(req, "Authorization");
    const char *content_type = get_header(req, "Content-Type");
    const char *custom = get_header(req, "X-Custom-Header");
    
    char *response = arena_sprintf(req->arena, "auth=%s,ct=%s,custom=%s",
        auth ? auth : "null",
        content_type ? content_type : "null",
        custom ? custom : "null");
    
    send_text(res, 200, response);
}

int test_request_headers(void)
{
    MockHeaders headers[] = {
        {"Authorization", "Bearer token123"},
        {"Content-Type", "application/json"},
        {"X-Custom-Header", "custom-value"}
    };
    
    MockParams params = {
        .method = MOCK_GET,
        .path = "/headers",
        .headers = headers,
        .header_count = 3
    };
    
    MockResponse res = request(&params);
    
    ASSERT_EQ(200, res.status_code);
    ASSERT_EQ_STR("auth=Bearer token123,ct=application/json,custom=custom-value", res.body);
    
    free_request(&res);
    RETURN_OK();
}
