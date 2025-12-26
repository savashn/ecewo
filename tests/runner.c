#include "ecewo.h"
#include "ecewo-mock.h"
#include "test-handlers.h"
#include "tester.h"

void setup_routes(void)
{
    // test-params
    get("/param/:id1/:id2/:id3/:id4/:id5/:id6/:id7/:id8/:id9/:id10", handler_overflow_param);
    get("/users/:userId/posts/:postId/comments/:commentId", handler_multi_param);
    get("/users/:userId", handler_single_param);

    // test-query
    get("/search", handler_query_params);

    // test-methods
    get("/method", handler_method_echo);
    post("/method", handler_method_echo);
    put("/method", handler_method_echo);
    del("/method", handler_method_echo);
    patch("/method", handler_method_echo);
    post("/echo-body", handler_post_body);

    // test-middleware
    get("/mw-order", middleware_first, middleware_second, middleware_third, handler_middleware_order);
    get("/mw-abort", middleware_abort, handler_should_not_reach);

    // test-context
    get("/context", context_middleware, context_handler);

    // test-response
    get("/json-response", handler_json_response);
    get("/html-response", handler_html_response);
    get("/status", handler_status_codes);

    // test-headers
    get("/headers", handler_echo_headers);
    get("/custom-headers", handler_set_headers);
    get("/header-injection", handler_header_injection);

    // test-body
    post("/large-body", handler_large_body);

    // test-redirect
    get("/old-path", handler_redirect);
    get("/new-location", handler_new_location);
    get("/redirect-injection", handler_header_injection);
    
    // test-concurrent request
    get("/counter", handler_counter);
    
    // test-spawn
    get("/compute", handler_compute);
    post("/background", handler_fire_and_forget);
    get("/check-counter", handler_check_counter);
    get("/parallel", handler_parallel);

    // test-blocking
    get("/thread-test", handler_thread_test);
    get("/main-thread", handler_get_main_thread);
    get("/fast", handler_fast);
    get("/slow", handler_slow);
    
    // test-root
    get("/", handler_root);
}

int main(void)
{
    mock_init(setup_routes);

    // Route Parameters
    RUN_TEST(test_single_param);
    RUN_TEST(test_multi_param);
    RUN_TEST(test_param_special_chars);
    RUN_TEST(test_overflow_param);

    // Query String
    RUN_TEST(test_query_multiple);
    RUN_TEST(test_query_empty_value);
    RUN_TEST(test_query_no_params);

    // HTTP Methods
    RUN_TEST(test_method_get);
    RUN_TEST(test_method_post);
    RUN_TEST(test_method_put);
    RUN_TEST(test_method_delete);
    RUN_TEST(test_method_patch);
    RUN_TEST(test_post_with_body);

    // Middleware
    RUN_TEST(test_middleware_execution_order);
    RUN_TEST(test_middleware_abort);

    // Middleware Context
    RUN_TEST(test_context);

    // Response Types
    RUN_TEST(test_json_content_type);
    RUN_TEST(test_html_content_type);
    RUN_TEST(test_status_201);
    RUN_TEST(test_status_404);
    RUN_TEST(test_status_500);
    RUN_TEST(test_404_unknown_path);
    RUN_TEST(test_404_wrong_method);

    // Request Headers
    RUN_TEST(test_request_headers);
    RUN_TEST(test_set_headers);
    RUN_TEST(test_header_injection);

    // Large Body Handling
    RUN_TEST(test_large_body);

    // Redirect
    RUN_TEST(test_redirect);
    RUN_TEST(test_header_injection);
    
    // Sequential Request
    RUN_TEST(test_sequential_requests);
    
    // Spawn
    RUN_TEST(test_spawn_with_response);
    RUN_TEST(test_spawn_fire_and_forget);
    RUN_TEST(test_spawn_parallel);

    // Blocking and Non-Blocking
    RUN_TEST(test_spawn_thread_ids);
    RUN_TEST(test_spawn_not_blocking);
    RUN_TEST(test_sync_blocking);
    
    // Root
    RUN_TEST(test_root_path);

    mock_cleanup();

    return 0;
}
