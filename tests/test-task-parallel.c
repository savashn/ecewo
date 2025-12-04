#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"

typedef struct {
    Res *res;
    int total;
    int completed;
    int results[3];
    bool has_error;
} parallel_ctx_t;

static void parallel_work_1(void *context)
{
    parallel_ctx_t *ctx = context;
    ctx->results[0] = 10;
}

static void parallel_work_2(void *context)
{
    parallel_ctx_t *ctx = context;
    ctx->results[1] = 20;
}

static void parallel_work_3(void *context)
{
    parallel_ctx_t *ctx = context;
    ctx->results[2] = 30;
}

static void parallel_done(void *context)
{
    parallel_ctx_t *ctx = context;
    
    ctx->completed++;
    
    if (ctx->has_error && ctx->completed == 1)
    {
        send_text(ctx->res, 500, "spawn failed");
        return;
    }
    
    if (ctx->completed == ctx->total && !ctx->has_error)
    {
        int sum = ctx->results[0] + ctx->results[1] + ctx->results[2];
        char *response = arena_sprintf(ctx->res->arena, "{\"sum\":%d}", sum);
        send_json(ctx->res, 200, response);
    }
}

void handler_parallel(Req *req, Res *res)
{
    parallel_ctx_t *ctx = arena_alloc(req->arena, sizeof(parallel_ctx_t));
    ctx->res = res;
    ctx->total = 3;
    ctx->completed = 0;
    ctx->results[0] = 0;
    ctx->results[1] = 0;
    ctx->results[2] = 0;
    ctx->has_error = false;
    
    spawn(ctx, parallel_work_1, parallel_done);
    spawn(ctx, parallel_work_2, parallel_done);
    spawn(ctx, parallel_work_3, parallel_done);
}

int test_spawn_parallel(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/parallel"
    };
    
    MockResponse res = request(&params);
    
    ASSERT_EQ(200, res.status_code);
    ASSERT_EQ_STR("{\"sum\":60}", res.body);
    
    free_request(&res);
    RETURN_OK();
}
