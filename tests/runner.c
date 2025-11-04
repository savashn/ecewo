#include "unity.h"
#include "ecewo.h"
#include "ecewo/mock.h"
#include "test-handlers.h"

void setUp(void)
{
    // No need to per-test setup
}

void tearDown(void)
{
    // No need to per-test teardown
}

void suiteSetUp(void)
{
    ecewo_test_setup();
}

int suiteTearDown(int num_failures)
{
    ecewo_test_tear_down(num_failures);
}

void setup_routes(void)
{
    // test-request
    get("/users/:userId/posts/:postId", handler_params);
    get("/search", handler_query);
    post("/headers", handler_headers);

    // test-basics
    get("/plaintext", handler_plaintext);
    get("/json", handler_json);
    get("/html", handler_html);

    // test-context
    get("/context", use(context_middleware), context_handler);

    // blocking/non-blocking
    get("/instant", instant_handler);
    get_worker("/slow-async", use(slow_middleware), slow_async_handler);
    get("/fast-sync", use(slow_middleware), fast_sync_handler);
}

int main(void)
{
    test_routes_hook(setup_routes);
    suiteSetUp();
    UNITY_BEGIN();

    // test-request
    RUN_TEST(test_params);
    RUN_TEST(test_query);
    RUN_TEST(test_headers);

    // test-basics
    RUN_TEST(test_plaintext);
    RUN_TEST(test_json);
    RUN_TEST(test_html);

    // test-context
    RUN_TEST(test_context);

    // test-worker
    RUN_TEST(test_not_blocked);
    RUN_TEST(test_sync_blocks);

    // test cluster
    RUN_TEST(test_cluster_cpu_count);
    RUN_TEST(test_cluster_config_defaults);
    RUN_TEST(test_cluster_callbacks);
    RUN_TEST(test_cluster_invalid_config);
    
#ifdef _WIN32
    RUN_TEST(test_cluster_windows_port_strategy);
#else
    RUN_TEST(test_cluster_unix_port_strategy);
#endif
    
    int result = UNITY_END();

    suiteTearDown(result);
    return result;
}
