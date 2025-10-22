#include "ecewo.h"
#include "mock.h"
#include "unity.h"
#include <string.h>

typedef struct {
    char *user_id;
    char *role;
} user_ctx_t;

int context_middleware(Req *req, Res *res, Chain *chain)
{
    const char *token = get_header(req, "Authorization");
    
    user_ctx_t *ctx = ecewo_alloc(req, sizeof(user_ctx_t));

    ctx->user_id = ecewo_strdup(req, "user123");
    ctx->role = ecewo_strdup(req, "admin");
    
    set_context(req, "user_ctx", ctx, sizeof(user_ctx_t));
    
    return next(req, res, chain);
}

void context_handler(Req *req, Res *res)
{
    user_ctx_t *ctx = (user_ctx_t *)get_context(req, "user_ctx");

    if (strcmp(ctx->user_id, "user123") != 0 ||
        strcmp(ctx->role, "admin") != 0)
    {
        send_text(res, FORBIDDEN, "Forbidden");
        return;
    }
    
    send_text(res, 200, "Success!");
}

void test_context(void)
{
    MockHeaders headers[] = {
        {"Authorization", "Bearer token"}
    };

    MockParams params = {
        .method = GET,
        .path = "/context",
        .headers = headers,
        .header_count = 1
    };

    MockResponse res = request(&params);
    
    TEST_ASSERT_EQUAL(200, res.status_code);
    TEST_ASSERT_NOT_NULL(res.body);
    TEST_ASSERT_EQUAL_STRING("Success!", res.body);

    free_request(&res);
}
