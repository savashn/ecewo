#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"
#include <string.h>

typedef struct
{
    char *user_id;
    char *role;
} user_ctx_t;

void context_middleware(Req *req, Res *res, Next next)
{
    user_ctx_t *ctx = arena_alloc(req->arena, sizeof(user_ctx_t));
    ctx->user_id = arena_strdup(req->arena, "user123");
    ctx->role = arena_strdup(req->arena, "admin");
    
    set_context(req, "user_ctx", ctx);
    
    next(req, res);
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

int test_context(void)
{
    MockHeaders headers[] = {
        {"Authorization", "Bearer token"}
    };

    MockParams params = {
        .method = MOCK_GET,
        .path = "/context",
        .headers = headers,
        .header_count = 1
    };

    MockResponse res = request(&params);
    
    ASSERT_EQ(200, res.status_code);
    ASSERT_NOT_NULL(res.body);
    ASSERT_EQ_STR("Success!", res.body);

    free_request(&res);

    RETURN_OK();
}
