#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"
#include "uv.h"

typedef struct
{
    Req *req;
    Res *res;
    Next next;
} mw_ctx_t;

typedef struct
{
    char *user_id;
    char *role;
} user_ctx_t;

static void auth_work(void *context)
{
    (void)context;
    uv_sleep(100);
}

static void auth_done(void *context)
{
    mw_ctx_t *ctx = context;
    
    user_ctx_t *user = arena_alloc(ctx->req->arena, sizeof(user_ctx_t));
    user->user_id = arena_strdup(ctx->req->arena, "user123");
    user->role = arena_strdup(ctx->req->arena, "admin");
    
    set_context(ctx->req, "user", user);
    
    ctx->next(ctx->req, ctx->res);
}

void middleware_async_auth(Req *req, Res *res, Next next)
{
    const char *token = get_header(req, "Authorization");
    
    if (!token)
    {
        send_text(res, 401, "Unauthorized");
        return;
    }
    
    mw_ctx_t *ctx = arena_alloc(req->arena, sizeof(mw_ctx_t));
    ctx->req = req;
    ctx->res = res;
    ctx->next = next;
    
    spawn(ctx, auth_work, auth_done);
}

void handler_protected(Req *req, Res *res)
{
    user_ctx_t *user = get_context(req, "user");
    
    if (!user)
    {
        send_text(res, 500, "Internal Server Error");
        return;
    }
    
    char *response = arena_sprintf(req->arena,
        "Welcome %s (role: %s)",
        user->user_id,
        user->role
    );
    
    send_text(res, 200, response);
}

int test_async_auth_middleware(void)
{
    MockHeaders headers[] = {
        {"Authorization", "Bearer token123"}
    };
    
    MockParams params = {
        .method = MOCK_GET,
        .path = "/mw-async",
        .headers = headers,
        .header_count = 1
    };
    
    MockResponse res = request(&params);
    
    ASSERT_EQ(200, res.status_code);
    ASSERT_EQ_STR("Welcome user123 (role: admin)", res.body);
    
    free_request(&res);
    RETURN_OK();
}

int test_async_auth_no_token(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/mw-async"
    };
    
    MockResponse res = request(&params);
    
    ASSERT_EQ(401, res.status_code);
    ASSERT_EQ_STR("Unauthorized", res.body);
    
    free_request(&res);
    RETURN_OK();
}
