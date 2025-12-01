#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"

typedef struct {
    Req *req;
    Res *res;
    int result;
} compute_ctx_t;

static void compute_work(Task *task, void *context)
{
    (void)task;
    compute_ctx_t *ctx = context;
    ctx->result = 42 * 2;
}

static void compute_done(void *context, char *error)
{
    compute_ctx_t *ctx = context;
    
    if (error)
    {
        send_text(ctx->res, 500, error);
        return;
    }
    
    char *response = ecewo_sprintf(ctx->res, "result=%d", ctx->result);
    send_text(ctx->res, 200, response);
}

void handler_compute(Req *req, Res *res)
{
    compute_ctx_t *ctx = ecewo_alloc(req, sizeof(compute_ctx_t));
    ctx->req = req;
    ctx->res = res;
    ctx->result = 0;
    
    task(ctx, compute_work, compute_done);
}

int test_task_with_response(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/compute"
    };
    
    MockResponse res = request(&params);
    
    ASSERT_EQ(200, res.status_code);
    ASSERT_NOT_NULL(res.body);
    ASSERT_EQ_STR("result=84", res.body);
    
    free_request(&res);
    RETURN_OK();
}
