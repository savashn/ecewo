#include "tester.h"
#include "ecewo.h"
#include "ecewo-mock.h"
#include "uv.h"
#include <stdlib.h>

static inline uint64_t get_time_ms(void)
{
    return uv_hrtime() / 1000000;
}

static inline unsigned long get_thread_id(void)
{
    return (unsigned long)uv_thread_self();
}

// ============================================================================
// Task Thread ID Test
// ============================================================================

typedef struct {
    Res *res;
    unsigned long main_thread_id;
    unsigned long work_thread_id;
    unsigned long done_thread_id;
} thread_test_ctx_t;

static void thread_test_work(void *context)
{
    thread_test_ctx_t *ctx = context;
    ctx->work_thread_id = get_thread_id();
    uv_sleep(100);
}

static void thread_test_done(void *context)
{
    thread_test_ctx_t *ctx = context;
    ctx->done_thread_id = get_thread_id();
    
    char *response = arena_sprintf(ctx->res->arena, "%lu,%lu,%lu",
        ctx->main_thread_id,
        ctx->work_thread_id,
        ctx->done_thread_id);
    
    send_text(ctx->res, 200, response);
}

void handler_thread_test(Req *req, Res *res)
{
    thread_test_ctx_t *ctx = arena_alloc(res->arena, sizeof(thread_test_ctx_t));
    ctx->res = res;
    ctx->main_thread_id = get_thread_id();
    ctx->work_thread_id = 0;
    ctx->done_thread_id = 0;
    
    task(ctx, thread_test_work, thread_test_done);
}

void handler_get_main_thread(Req *req, Res *res)
{
    (void)req;
    char *response = arena_sprintf(res->arena, "%lu", get_thread_id());
    send_text(res, 200, response);
}

void handler_fast(Req *req, Res *res)
{
    (void)req;
    send_text(res, 200, "fast");
}

void handler_slow_sync(Req *req, Res *res)
{
    (void)req;
    uv_sleep(300);
    send_text(res, 200, "slow-sync");
}

// ============================================================================
// Background Request Helper
// ============================================================================

typedef struct {
    const char *path;
    MockResponse response;
    uint64_t duration_ms;
} async_test_ctx_t;

static void background_request(void *arg)
{
    async_test_ctx_t *ctx = (async_test_ctx_t *)arg;
    uint64_t start = get_time_ms();
    MockParams params = {.method = MOCK_GET, .path = ctx->path};
    ctx->response = request(&params);
    ctx->duration_ms = get_time_ms() - start;
}

// ============================================================================
// Tests
// ============================================================================

int test_task_thread_ids(void)
{
    MockParams main_params = {.method = MOCK_GET, .path = "/main-thread"};
    MockResponse main_res = request(&main_params);
    ASSERT_EQ(200, main_res.status_code);
    
    unsigned long server_main_thread = strtoul(main_res.body, NULL, 10);
    printf("\n  Server main thread: %lu\n", server_main_thread);
    free_request(&main_res);
    
    MockParams task_params = {.method = MOCK_GET, .path = "/thread-test"};
    MockResponse task_res = request(&task_params);
    ASSERT_EQ(200, task_res.status_code);
    ASSERT_NOT_NULL(task_res.body);
    
    unsigned long handler_tid, work_tid, done_tid;
    int parsed = sscanf(task_res.body, "%lu,%lu,%lu", &handler_tid, &work_tid, &done_tid);
    ASSERT_EQ(3, parsed);
    
    printf("  Handler thread: %lu\n", handler_tid);
    printf("  Work thread:    %lu\n", work_tid);
    printf("  Done thread:    %lu\n", done_tid);
    
    ASSERT_EQ(server_main_thread, handler_tid);
    ASSERT_NE(server_main_thread, work_tid);
    ASSERT_EQ(server_main_thread, done_tid);
    
    free_request(&task_res);
    RETURN_OK();
}

int test_task_not_blocking(void)
{
    async_test_ctx_t slow_ctx = {.path = "/thread-test"};

    uv_thread_t thread;
    uv_thread_create(&thread, background_request, &slow_ctx);

    uv_sleep(30);

    uint64_t fast_start = get_time_ms();
    MockParams fast_params = {.method = MOCK_GET, .path = "/fast"};
    MockResponse fast_res = request(&fast_params);
    uint64_t fast_duration = get_time_ms() - fast_start;

    uv_thread_join(&thread);

    printf("\n  Task request: %lu ms\n", (unsigned long)slow_ctx.duration_ms);
    printf("  Fast request: %lu ms (should be <50ms)\n", (unsigned long)fast_duration);

    ASSERT_EQ(200, slow_ctx.response.status_code);
    ASSERT_EQ(200, fast_res.status_code);
    ASSERT_TRUE(fast_duration < 50);

    free_request(&slow_ctx.response);
    free_request(&fast_res);
    RETURN_OK();
}

int test_sync_blocking(void)
{
    async_test_ctx_t slow_ctx = {.path = "/slow-sync"};

    uv_thread_t thread;
    uv_thread_create(&thread, background_request, &slow_ctx);

    uv_sleep(30);

    uint64_t fast_start = get_time_ms();
    MockParams fast_params = {.method = MOCK_GET, .path = "/fast"};
    MockResponse fast_res = request(&fast_params);
    uint64_t fast_duration = get_time_ms() - fast_start;

    uv_thread_join(&thread);

    printf("\n  Sync request: %lu ms\n", (unsigned long)slow_ctx.duration_ms);
    printf("  Fast request: %lu ms (should be >=200ms)\n", (unsigned long)fast_duration);

    ASSERT_EQ(200, slow_ctx.response.status_code);
    ASSERT_EQ(200, fast_res.status_code);
    ASSERT_TRUE(fast_duration >= 200);

    free_request(&slow_ctx.response);
    free_request(&fast_res);
    RETURN_OK();
}
