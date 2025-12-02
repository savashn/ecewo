#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"
#include "uv.h"

static int background_counter = 0;

typedef struct {
    int increment;
} background_ctx_t;

static void background_work(Task *task, void *context)
{
    (void)task;
    background_ctx_t *ctx = context;
    background_counter += ctx->increment;
}

static void background_done(void *context, char *error)
{
    (void)error;
    background_ctx_t *ctx = context;
    free(ctx);
}

void handler_fire_and_forget(Req *req, Res *res)
{
    (void)req;
    
    background_ctx_t *ctx = malloc(sizeof(background_ctx_t));
    ctx->increment = 10;
    
    task(ctx, background_work, background_done);
    
    send_json(res, 202, "{\"status\":\"accepted\"}");
}

void handler_check_counter(Req *req, Res *res)
{
    char *response = arena_sprintf(req->arena, "{\"counter\":%d}", background_counter);
    send_json(res, 200, response);
}

int test_task_fire_and_forget(void)
{
    background_counter = 0;
    
    MockParams params1 = {
        .method = MOCK_POST,
        .path = "/background"
    };
    
    MockResponse res1 = request(&params1);
    ASSERT_EQ(202, res1.status_code);
    ASSERT_EQ_STR("{\"status\":\"accepted\"}", res1.body);
    free_request(&res1);
    
    uv_sleep(100);
    
    MockParams params2 = {
        .method = MOCK_GET,
        .path = "/check-counter"
    };
    
    MockResponse res2 = request(&params2);
    ASSERT_EQ(200, res2.status_code);
    ASSERT_EQ_STR("{\"counter\":10}", res2.body);
    free_request(&res2);
    
    RETURN_OK();
}
