#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "test_framework.h"
#include "ecewo.h"
#include "router.h"
#include "request.h"
#include "route_trie.h"
#include "middleware.h"
#include "cors.h"
#include "cookie.h"
#include "server.h"
// #include "async.h"

// Global test counters
int tests_passed = 0;
int tests_failed = 0;
int total_tests = 0;

// Global test state
int middleware_call_count = 0;
int handler_call_count = 0;

// Test helper functions
void cleanup_request_t(request_t *req) {
    if (!req || !req->items) return;
    
    for (int i = 0; i < req->count; i++) {
        free(req->items[i].key);
        free(req->items[i].value);
    }
    free(req->items);
    memset(req, 0, sizeof(request_t));
}

// ========== ROUTE TRIE TESTS ==========

int test_route_trie_creation() {
    route_trie_t *trie = route_trie_create();
    TEST_ASSERT(trie != NULL, "Route trie should be created successfully");
    TEST_ASSERT(trie->root != NULL, "Route trie should have a root node");
    TEST_ASSERT(trie->route_count == 0, "New trie should have zero routes");
    
    route_trie_free(trie);
    TEST_PASS();
}

int test_route_trie_simple_routes() {
    route_trie_t *trie = route_trie_create();
    
    // Simple handler for testing
    void simple_handler(Req *req, Res *res) {
        (void)req; (void)res;
    }
    
    // Add simple routes
    int result1 = route_trie_add(trie, "GET", "/", simple_handler, NULL);
    int result2 = route_trie_add(trie, "GET", "/users", simple_handler, NULL);
    int result3 = route_trie_add(trie, "POST", "/users", simple_handler, NULL);
    
    TEST_ASSERT(result1 == 0, "Adding root route should succeed");
    TEST_ASSERT(result2 == 0, "Adding /users GET route should succeed");
    TEST_ASSERT(result3 == 0, "Adding /users POST route should succeed");
    TEST_ASSERT(trie->route_count == 3, "Trie should have 3 routes");
    
    route_trie_free(trie);
    TEST_PASS();
}

int test_route_trie_parameter_routes() {
    route_trie_t *trie = route_trie_create();
    
    void param_handler(Req *req, Res *res) {
        (void)req; (void)res;
    }
    
    // Add parameter routes
    int result1 = route_trie_add(trie, "GET", "/users/:id", param_handler, NULL);
    int result2 = route_trie_add(trie, "GET", "/users/:id/posts/:post_id", param_handler, NULL);
    
    TEST_ASSERT(result1 == 0, "Adding parameter route should succeed");
    TEST_ASSERT(result2 == 0, "Adding nested parameter route should succeed");
    
    route_trie_free(trie);
    TEST_PASS();
}

int test_route_matching() {
    route_trie_t *trie = route_trie_create();
    
    void handler(Req *req, Res *res) {
        (void)req; (void)res;
    }
    
    // Add test routes
    route_trie_add(trie, "GET", "/", handler, NULL);
    route_trie_add(trie, "GET", "/users/:id", handler, NULL);
    route_trie_add(trie, "GET", "/static/*", handler, NULL);
    
    // Test matching
    tokenized_path_t path;
    route_match_t match;
    
    // Test root route
    tokenize_path("/", &path);
    bool found1 = route_trie_match(trie, "GET", &path, &match);
    TEST_ASSERT(found1 == true, "Root route should be found");
    free_tokenized_path(&path);
    
    // Test parameter route
    tokenize_path("/users/123", &path);
    bool found2 = route_trie_match(trie, "GET", &path, &match);
    TEST_ASSERT(found2 == true, "Parameter route should be found");
    TEST_ASSERT(match.param_count == 1, "Should have one parameter");
    free_tokenized_path(&path);
    
    // Test wildcard route
    tokenize_path("/static/css/style.css", &path);
    bool found3 = route_trie_match(trie, "GET", &path, &match);
    TEST_ASSERT(found3 == true, "Wildcard route should be found");
    free_tokenized_path(&path);
    
    // Test non-existent route
    tokenize_path("/nonexistent", &path);
    bool found4 = route_trie_match(trie, "GET", &path, &match);
    TEST_ASSERT(found4 == false, "Non-existent route should not be found");
    free_tokenized_path(&path);
    
    route_trie_free(trie);
    TEST_PASS();
}

// ========== REQUEST PARSING TESTS ==========

int test_http_context_initialization() {
    http_context_t context;
    http_context_init(&context);
    
    TEST_ASSERT(context.url != NULL, "URL buffer should be allocated");
    TEST_ASSERT(context.method != NULL, "Method buffer should be allocated");
    TEST_ASSERT(context.body != NULL, "Body buffer should be allocated");
    TEST_ASSERT(context.headers.items != NULL, "Headers array should be allocated");
    TEST_ASSERT(context.url_capacity > 0, "URL capacity should be positive");
    TEST_ASSERT(context.method_capacity > 0, "Method capacity should be positive");
    
    http_context_free(&context);
    TEST_PASS();
}

int test_query_parsing() {
    request_t query;
    
    // Test simple query
    parse_query("name=john&age=25", &query);
    TEST_ASSERT(query.count == 2, "Should parse 2 query parameters");
    
    const char *name = get_req(&query, "name");
    const char *age = get_req(&query, "age");
    TEST_ASSERT(name != NULL && strcmp(name, "john") == 0, "Name should be 'john'");
    TEST_ASSERT(age != NULL && strcmp(age, "25") == 0, "Age should be '25'");
    
    cleanup_request_t(&query);
    
    // Test empty query
    parse_query("", &query);
    TEST_ASSERT(query.count == 0, "Empty query should have 0 parameters");
    cleanup_request_t(&query);
    
    // Test single parameter
    parse_query("single=value", &query);
    TEST_ASSERT(query.count == 1, "Single parameter query should have 1 parameter");
    const char *single = get_req(&query, "single");
    TEST_ASSERT(single != NULL && strcmp(single, "value") == 0, "Single value should be 'value'");
    cleanup_request_t(&query);
    
    TEST_PASS();
}

int test_path_tokenization() {
    tokenized_path_t path;
    
    // Test simple path
    int result1 = tokenize_path("/users/123", &path);
    TEST_ASSERT(result1 == 0, "Tokenization should succeed");
    TEST_ASSERT(path.count == 2, "Path should have 2 segments");
    TEST_ASSERT(strncmp(path.segments[0].start, "users", path.segments[0].len) == 0, "First segment should be 'users'");
    TEST_ASSERT(strncmp(path.segments[1].start, "123", path.segments[1].len) == 0, "Second segment should be '123'");
    free_tokenized_path(&path);
    
    // Test root path
    int result2 = tokenize_path("/", &path);
    TEST_ASSERT(result2 == 0, "Root path tokenization should succeed");
    TEST_ASSERT(path.count == 0, "Root path should have 0 segments");
    free_tokenized_path(&path);
    
    // Test parameter path
    int result3 = tokenize_path("/users/:id", &path);
    TEST_ASSERT(result3 == 0, "Parameter path tokenization should succeed");
    TEST_ASSERT(path.count == 2, "Parameter path should have 2 segments");
    TEST_ASSERT(path.segments[1].is_param == true, "Second segment should be a parameter");
    free_tokenized_path(&path);
    
    TEST_PASS();
}

// ========== MIDDLEWARE TESTS ==========

int test_middleware1(Req *req, Res *res, Chain *chain) {
    (void)req; (void)res;
    middleware_call_count++;
    return next(chain, req, res);
}

int test_middleware2(Req *req, Res *res, Chain *chain) {
    (void)req; (void)res;
    middleware_call_count++;
    return next(chain, req, res);
}

void test_handler(Req *req, Res *res) {
    (void)req; (void)res;
    handler_call_count++;
}

int test_middleware_execution() {
    // Reset counters
    middleware_call_count = 0;
    handler_call_count = 0;
    
    // Initialize router
    init_router();
    
    // Add global middleware
    hook(test_middleware1);
    hook(test_middleware2);
    
    // Register route with middleware
    MiddlewareArray mw = use(test_middleware1);
    register_route("GET", "/test", mw, test_handler);
    
    TEST_ASSERT(global_middleware_count == 2, "Should have 2 global middleware");
    
    reset_router();
    TEST_PASS();
}

// ========== COOKIE TESTS ==========

int test_cookie_parsing() {
    // Create a mock request with cookies
    Req req;
    memset(&req, 0, sizeof(Req));
    
    // Initialize headers
    req.headers.capacity = 1;
    req.headers.count = 1;
    req.headers.items = malloc(sizeof(request_item_t));
    req.headers.items[0].key = strdup("Cookie");
    req.headers.items[0].value = strdup("session_id=abc123; user_pref=dark_mode");
    
    char *session_id = get_cookie(&req, "session_id");
    char *user_pref = get_cookie(&req, "user_pref");
    char *nonexistent = get_cookie(&req, "nonexistent");
    
    TEST_ASSERT(session_id != NULL && strcmp(session_id, "abc123") == 0, "Session ID should be 'abc123'");
    TEST_ASSERT(user_pref != NULL && strcmp(user_pref, "dark_mode") == 0, "User pref should be 'dark_mode'");
    TEST_ASSERT(nonexistent == NULL, "Non-existent cookie should return NULL");
    
    // Cleanup
    free(session_id);
    free(user_pref);
    cleanup_request_t(&req.headers);
    
    TEST_PASS();
}

// ========== RESPONSE TESTS ==========

int test_header_setting() {
    Res res;
    memset(&res, 0, sizeof(Res));
    
    set_header(&res, "Content-Type", "application/json");
    set_header(&res, "X-Custom-Header", "custom-value");
    
    TEST_ASSERT(res.header_count == 2, "Should have 2 headers");
    TEST_ASSERT(strcmp(res.headers[0].name, "Content-Type") == 0, "First header name should be Content-Type");
    TEST_ASSERT(strcmp(res.headers[0].value, "application/json") == 0, "First header value should be application/json");
    TEST_ASSERT(strcmp(res.headers[1].name, "X-Custom-Header") == 0, "Second header name should be X-Custom-Header");
    TEST_ASSERT(strcmp(res.headers[1].value, "custom-value") == 0, "Second header value should be custom-value");
    
    // Cleanup
    for (int i = 0; i < res.header_count; i++) {
        free(res.headers[i].name);
        free(res.headers[i].value);
    }
    free(res.headers);
    
    TEST_PASS();
}

// ========== UTILITY TESTS ==========

int test_http_method_detection() {
    TEST_ASSERT(get_method_index("GET") == METHOD_GET, "GET method should be detected");
    TEST_ASSERT(get_method_index("POST") == METHOD_POST, "POST method should be detected");
    TEST_ASSERT(get_method_index("PUT") == METHOD_PUT, "PUT method should be detected");
    TEST_ASSERT(get_method_index("DELETE") == METHOD_DELETE, "DELETE method should be detected");
    TEST_ASSERT(get_method_index("PATCH") == METHOD_PATCH, "PATCH method should be detected");
    TEST_ASSERT(get_method_index("HEAD") == METHOD_HEAD, "HEAD method should be detected");
    TEST_ASSERT(get_method_index("OPTIONS") == METHOD_OPTIONS, "OPTIONS method should be detected");
    TEST_ASSERT(get_method_index("INVALID") == METHOD_UNKNOWN, "Invalid method should return UNKNOWN");
    TEST_ASSERT(get_method_index(NULL) == METHOD_UNKNOWN, "NULL method should return UNKNOWN");
    
    TEST_PASS();
}

// ========== INTEGRATION TESTS ==========

int test_full_route_registration() {
    init_router();
    
    void test_get_handler(Req *req, Res *res) { (void)req; (void)res; }
    void test_post_handler(Req *req, Res *res) { (void)req; (void)res; }
    
    // Test route registration
    register_route("GET", "/", NO_MW, test_get_handler);
    register_route("POST", "/users", NO_MW, test_post_handler);
    
    TEST_ASSERT(global_route_trie != NULL, "Global route trie should be initialized");
    TEST_ASSERT(global_route_trie->route_count == 2, "Should have 2 registered routes");
    
    reset_router();
    TEST_PASS();
}

// ========== MAIN TEST RUNNER ==========

void print_test_summary() {
    printf("\n================================================\n");
    printf("TEST SUMMARY\n");
    printf("================================================\n");
    printf("Total Tests: %d\n", total_tests);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    if (total_tests > 0) {
        printf("Success Rate: %.1f%%\n", (tests_passed * 100.0) / total_tests);
    }
    printf("================================================\n");
}

int main() {
    printf("ECEWO Web Framework Test Suite\n");
    printf("================================================\n");
    
    // Route Trie Tests
    printf("\n[ROUTE TRIE TESTS]\n");
    RUN_TEST(test_route_trie_creation);
    RUN_TEST(test_route_trie_simple_routes);
    RUN_TEST(test_route_trie_parameter_routes);
    RUN_TEST(test_route_matching);
    
    // Request Parsing Tests
    printf("\n[REQUEST PARSING TESTS]\n");
    RUN_TEST(test_http_context_initialization);
    RUN_TEST(test_query_parsing);
    RUN_TEST(test_path_tokenization);
    
    // Middleware Tests
    printf("\n[MIDDLEWARE TESTS]\n");
    RUN_TEST(test_middleware_execution);
    
    // Cookie Tests
    printf("\n[COOKIE TESTS]\n");
    RUN_TEST(test_cookie_parsing);
    
    // Response Tests
    printf("\n[RESPONSE TESTS]\n");
    RUN_TEST(test_header_setting);
    
    // Utility Tests
    printf("\n[UTILITY TESTS]\n");
    RUN_TEST(test_http_method_detection);
    
    // Integration Tests
    printf("\n[INTEGRATION TESTS]\n");
    RUN_TEST(test_full_route_registration);
    
    // Additional Unit Tests
    run_unit_tests();
    
    print_test_summary();
    
    return tests_failed == 0 ? 0 : 1;
}