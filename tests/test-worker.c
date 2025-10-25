#include "unity.h"
#include "ecewo.h"
#include "ecewo/mock.h"
#include "uv.h"
#include "test-handlers.h"
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #define sleep_ms(ms) Sleep(ms)
    #define get_thread_id() GetCurrentThreadId()

    static inline uint64_t get_time_ms(void)
    {
        return GetTickCount64();
    }
#else
    #include <unistd.h>
    #define sleep_ms(ms) usleep((ms) * 1000)

    #include <pthread.h>
    #define get_thread_id() pthread_self()

    #include <sys/time.h>
    static inline uint64_t get_time_ms(void)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
    }
#endif

// ============================================================================
// Thread ID Utilities
// ============================================================================

static uv_key_t thread_id_key;
static uv_once_t thread_id_key_once = UV_ONCE_INIT;

static void init_thread_id_key(void)
{
    uv_key_create(&thread_id_key);
}

static const char *thread_id_to_string(void)
{
    uv_once(&thread_id_key_once, init_thread_id_key);
    char *buffer = (char *)uv_key_get(&thread_id_key);
    
    if (!buffer)
    {
        buffer = (char *)malloc(32);
        if (!buffer)
            return "OOM";
        
        uv_key_set(&thread_id_key, buffer);
    }
    
    snprintf(buffer, 32, "%lu", (unsigned long)get_thread_id());
    return buffer;
}

static void cleanup_thread_id(void)
{
    char *buffer = (char *)uv_key_get(&thread_id_key);
    if (buffer)
    {
        free(buffer);
        uv_key_set(&thread_id_key, NULL);
    }
}

// ============================================================================
// Test Handlers
// ============================================================================

int slow_middleware(Req *req, Res *res, Chain *chain)
{
    (void)req;
    (void)res;
    sleep_ms(100);
    return next(req, res, chain);
}

void slow_async_handler(Req *req, Res *res)
{
    (void)req;
    sleep_ms(200);
    send_text(res, 200, thread_id_to_string());
    cleanup_thread_id();
}

void instant_handler(Req *req, Res *res)
{
    (void)req;
    send_text(res, 200, thread_id_to_string());
    cleanup_thread_id();
}

void fast_sync_handler(Req *req, Res *res)
{
    (void)req;
    send_text(res, 200, thread_id_to_string());
    cleanup_thread_id();
}

// ============================================================================
// Thread Request Helper
// ============================================================================

typedef struct {
    const char *path;
    MockResponse response;
} request_context_t;

#ifdef _WIN32
    static unsigned __stdcall request_thread(void *arg)
    {
        request_context_t *ctx = (request_context_t *)arg;
        MockParams params = {.method = MOCK_GET, .path = ctx->path};
        ctx->response = request(&params);
        return 0;
    }
#else
    static void *request_thread(void *arg)
    {
        request_context_t *ctx = (request_context_t *)arg;
        MockParams params = {.method = MOCK_GET, .path = ctx->path};
        ctx->response = request(&params);
        return NULL;
    }
#endif

// ============================================================================
// Tests
// ============================================================================

void test_not_blocked(void)
{
    printf("\n=== TEST: Async Does NOT Block ===\n");
    
    request_context_t async_ctx = {.path = "/slow-async"};
    
#ifdef _WIN32
    HANDLE h = (HANDLE)_beginthreadex(NULL, 0, request_thread, &async_ctx, 0, NULL);
#else
    pthread_t t;
    pthread_create(&t, NULL, request_thread, &async_ctx);
#endif

    sleep_ms(50);
    
    MockParams instant_params = {.method = MOCK_GET, .path = "/instant"};
    MockResponse res_instant = request(&instant_params);
    
#ifdef _WIN32
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
#else
    pthread_join(t, NULL);
#endif
    
    TEST_ASSERT_NOT_NULL(async_ctx.response.body);
    TEST_ASSERT_NOT_NULL(res_instant.body);
    
    int different = strcmp(async_ctx.response.body, res_instant.body) != 0;

    printf("Should use different threads (non-blocking)\n");
    printf("Result: async=%s, instant=%s -> %s\n",
           async_ctx.response.body,
           res_instant.body,
           different ? "OK" : "FAIL");

    TEST_ASSERT_TRUE_MESSAGE(different, "Should use different threads");
    
    free_request(&async_ctx.response);
    free_request(&res_instant);
}

void test_sync_blocks(void)
{
    printf("\n=== TEST: Sync BLOCKS ===\n");
    
    request_context_t sync_ctx = {.path = "/fast-sync"};
    
#ifdef _WIN32
    HANDLE h = (HANDLE)_beginthreadex(NULL, 0, request_thread, &sync_ctx, 0, NULL);
#else
    pthread_t t;
    pthread_create(&t, NULL, request_thread, &sync_ctx);
#endif

    sleep_ms(50);
    
    MockParams instant_params = {.method = MOCK_GET, .path = "/instant"};
    MockResponse res_instant = request(&instant_params);
    
#ifdef _WIN32
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
#else
    pthread_join(t, NULL);
#endif
    
    TEST_ASSERT_NOT_NULL(sync_ctx.response.body);
    TEST_ASSERT_NOT_NULL(res_instant.body);
    
    int same = strcmp(sync_ctx.response.body, res_instant.body) == 0;

    printf("Should use same thread (blocking)\n");
    printf("Result: sync=%s, instant=%s -> %s\n",
           sync_ctx.response.body, 
           res_instant.body,
           same ? "OK" : "FAIL");

    TEST_ASSERT_TRUE_MESSAGE(same, "Should use same thread (blocking)");
    
    free_request(&sync_ctx.response);
    free_request(&res_instant);
}