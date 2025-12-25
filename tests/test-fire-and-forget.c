#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"
#include "uv.h"

static int background_counter = 0;

typedef struct
{
    Arena *arena;
    int increment;
} background_ctx_t;

static void background_work(void *context)
{
    background_ctx_t *ctx = context;
    background_counter += ctx->increment;
    arena_return(ctx->arena);
}

void handler_fire_and_forget(Req *req, Res *res)
{
    Arena *bg_arena = arena_borrow();
    
    background_ctx_t *ctx = arena_alloc(bg_arena, sizeof(background_ctx_t));
    ctx->arena = bg_arena;
    ctx->increment = 10;
    
    spawn(ctx, background_work, NULL);
    send_text(res, ACCEPTED, "Status: Accepted");
}

void handler_check_counter(Req *req, Res *res)
{
    char *response = arena_sprintf(req->arena, "Counter: %d", background_counter);
    send_text(res, 200, response);
}

int test_spawn_fire_and_forget(void)
{
    background_counter = 0;
    
    MockParams params1 = {
        .method = MOCK_POST,
        .path = "/background"
    };
    
    MockResponse res1 = request(&params1);
    ASSERT_EQ(202, res1.status_code);
    ASSERT_EQ_STR("Status: Accepted", res1.body);
    free_request(&res1);
    
    uv_sleep(100);
    
    MockParams params2 = {
        .method = MOCK_GET,
        .path = "/check-counter"
    };
    
    MockResponse res2 = request(&params2);
    ASSERT_EQ(200, res2.status_code);
    ASSERT_EQ_STR("Counter: 10", res2.body);
    free_request(&res2);
    
    RETURN_OK();
}
