#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"
#include <string.h>

void handler_single_param(Req *req, Res *res)
{
    const char *id = get_param(req, "userId");
    if (!id) {
        send_text(res, 400, "Missing id");
        return;
    }
    char *response = arena_sprintf(req->arena, "id=%s", id);
    send_text(res, 200, response);
}

void handler_multi_param(Req *req, Res *res)
{
    const char *userId = get_param(req, "userId");
    const char *postId = get_param(req, "postId");
    const char *commentId = get_param(req, "commentId");
    
    if (!userId || !postId || !commentId) {
        send_text(res, 400, "Missing params");
        return;
    }
    
    char *response = arena_sprintf(req->arena, "%s/%s/%s", userId, postId, commentId);
    send_text(res, 200, response);
}

int test_single_param(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/users/42"
    };
    
    MockResponse res = request(&params);
    
    ASSERT_EQ(200, res.status_code);
    ASSERT_NOT_NULL(res.body);
    ASSERT_EQ_STR("id=42", res.body);
    
    free_request(&res);
    RETURN_OK();
}

int test_multi_param(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/users/100/posts/200/comments/300"
    };
    
    MockResponse res = request(&params);
    
    ASSERT_EQ(200, res.status_code);
    ASSERT_NOT_NULL(res.body);
    ASSERT_EQ_STR("100/200/300", res.body);
    
    free_request(&res);
    RETURN_OK();
}

int test_param_special_chars(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/users/test-user-123"
    };
    
    MockResponse res = request(&params);
    
    ASSERT_EQ(200, res.status_code);
    ASSERT_NOT_NULL(res.body);
    ASSERT_EQ_STR("id=test-user-123", res.body);
    
    free_request(&res);
    RETURN_OK();
}
