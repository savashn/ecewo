#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"

void handler_large_body(Req *req, Res *res)
{
    char *response = ecewo_sprintf(res, "received=%zu", req->body_len);
    send_text(res, 200, response);
}

int test_large_body(void)
{
    // 10KB body
    size_t size = 10 * 1024;
    char *large_body = malloc(size + 1);
    memset(large_body, 'A', size);
    large_body[size] = '\0';
    
    MockParams params = {
        .method = MOCK_POST,
        .path = "/large-body",
        .body = large_body
    };
    
    MockResponse res = request(&params);
    
    ASSERT_EQ(200, res.status_code);
    
    char expected[32];
    snprintf(expected, sizeof(expected), "received=%zu", size);
    ASSERT_EQ_STR(expected, res.body);
    
    free(large_body);
    free_request(&res);
    RETURN_OK();
}
