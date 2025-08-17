#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "ecewo.h"
#include "test_framework.h"
#include "cookie.h"

#define PERFORMANCE_ITERATIONS 10000

// Performance test macros
#define TIME_OPERATION(operation, description) \
    do { \
        clock_t start = clock(); \
        for (int i = 0; i < PERFORMANCE_ITERATIONS; i++) { \
            operation; \
        } \
        clock_t end = clock(); \
        double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC; \
        printf("%-40s: %.6f seconds (%d iterations)\n", description, time_taken, PERFORMANCE_ITERATIONS); \
        printf("%-40s: %.2f ops/sec\n", "", PERFORMANCE_ITERATIONS / time_taken); \
    } while(0)

void performance_route_trie_operations() {
    printf("\n[ROUTE TRIE PERFORMANCE]\n");
    printf("========================================\n");
    
    route_trie_t *trie = route_trie_create();
    void dummy_handler(Req *req, Res *res) { (void)req; (void)res; }
    
    // Test route addition performance
    TIME_OPERATION(
        {
            char path[50];
            snprintf(path, sizeof(path), "/test/route/%d", i);
            route_trie_add(trie, "GET", path, dummy_handler, NULL);
        },
        "Route addition"
    );
    
    // Test route matching performance
    tokenized_path_t path;
    route_match_t match;
    tokenize_path("/test/route/5000", &path);
    
    TIME_OPERATION(
        route_trie_match(trie, "GET", &path, &match),
        "Route matching"
    );
    
    free_tokenized_path(&path);
    
    // Test parameter route matching
    route_trie_add(trie, "GET", "/users/:id/posts/:post_id", dummy_handler, NULL);
    tokenize_path("/users/123/posts/456", &path);
    
    TIME_OPERATION(
        route_trie_match(trie, "GET", &path, &match),
        "Parameter route matching"
    );
    
    free_tokenized_path(&path);
    route_trie_free(trie);
}

void performance_request_parsing() {
    printf("\n[REQUEST PARSING PERFORMANCE]\n");
    printf("========================================\n");
    
    http_context_t context;
    
    TIME_OPERATION(
        {
            http_context_init(&context);
            http_context_free(&context);
        },
        "HTTP context init/free"
    );
    
    request_t query;
    TIME_OPERATION(
        {
            parse_query("param1=value1&param2=value2&param3=value3", &query);
            // Clean up
            for (int j = 0; j < query.count; j++) {
                free(query.items[j].key);
                free(query.items[j].value);
            }
            free(query.items);
            memset(&query, 0, sizeof(query));
        },
        "Query string parsing"
    );
    
    tokenized_path_t tokenized;
    TIME_OPERATION(
        {
            tokenize_path("/users/123/posts/456", &tokenized);
            free_tokenized_path(&tokenized);
        },
        "Path tokenization"
    );
    
    // Test complex query parsing
    const char *complex_query = "name=john&age=25&active=true&tags=web,api,rest&score=95.5";
    TIME_OPERATION(
        {
            parse_query(complex_query, &query);
            // Clean up
            for (int j = 0; j < query.count; j++) {
                free(query.items[j].key);
                free(query.items[j].value);
            }
            free(query.items);
            memset(&query, 0, sizeof(query));
        },
        "Complex query parsing"
    );
}

void performance_memory_operations() {
    printf("\n[MEMORY OPERATIONS PERFORMANCE]\n");
    printf("========================================\n");
    
    // Test string operations
    TIME_OPERATION(
        {
            char *str = strdup("test string for performance");
            free(str);
        },
        "String duplication"
    );
    
    // Test request_t operations
    TIME_OPERATION(
        {
            request_t req;
            memset(&req, 0, sizeof(req));
            req.capacity = 10;
            req.items = malloc(req.capacity * sizeof(request_item_t));
            free(req.items);
        },
        "Request structure allocation"
    );
    
    // Test Req/Res creation and destruction
    TIME_OPERATION(
        {
            Req *req = malloc(sizeof(Req));
            memset(req, 0, sizeof(Req));
            req->method = strdup("GET");
            req->path = strdup("/test");
            destroy_req(req);
        },
        "Req creation/destruction"
    );
    
    // Test header operations
    TIME_OPERATION(
        {
            Res res;
            memset(&res, 0, sizeof(Res));
            set_header(&res, "Content-Type", "application/json");
            set_header(&res, "Cache-Control", "no-cache");
            // Cleanup
            for (int j = 0; j < res.header_count; j++) {
                free(res.headers[j].name);
                free(res.headers[j].value);
            }
            free(res.headers);
        },
        "Header setting operations"
    );
}

void performance_middleware_operations() {
    printf("\n[MIDDLEWARE PERFORMANCE]\n");
    printf("========================================\n");
    
    // Mock middleware functions
    int mock_middleware1(Req *req, Res *res, Chain *chain) {
        (void)req; (void)res;
        return next(chain, req, res);
    }
    
    int mock_middleware2(Req *req, Res *res, Chain *chain) {
        (void)req; (void)res;
        return next(chain, req, res);
    }
    
    void mock_handler(Req *req, Res *res) {
        (void)req; (void)res;
    }
    
    // Test middleware chain execution
    MiddlewareHandler handlers[] = {mock_middleware1, mock_middleware2};
    Chain chain = {
        .handlers = handlers,
        .count = 2,
        .current = 0,
        .route_handler = mock_handler
    };
    
    Req req;
    Res res;
    memset(&req, 0, sizeof(Req));
    memset(&res, 0, sizeof(Res));
    
    TIME_OPERATION(
        {
            chain.current = 0; // Reset chain
            next(&chain, &req, &res);
        },
        "Middleware chain execution"
    );
}

void performance_cookie_operations() {
    printf("\n[COOKIE PERFORMANCE]\n");
    printf("========================================\n");
    
    Req req;
    memset(&req, 0, sizeof(Req));
    
    // Setup mock request with cookies
    req.headers.capacity = 1;
    req.headers.count = 1;
    req.headers.items = malloc(sizeof(request_item_t));
    req.headers.items[0].key = strdup("Cookie");
    req.headers.items[0].value = strdup("session_id=abc123; user_pref=dark_mode; lang=en; theme=light");
    
    TIME_OPERATION(
        {
            char *session = get_cookie(&req, "session_id");
            char *pref = get_cookie(&req, "user_pref");
            free(session);
            free(pref);
        },
        "Cookie parsing"
    );
    
    // Cleanup
    free(req.headers.items[0].key);
    free(req.headers.items[0].value);
    free(req.headers.items);
    
    // Test cookie setting
    Res res;
    memset(&res, 0, sizeof(Res));
    
    cookie_options_t opts = {
        .max_age = 3600,
        .path = "/",
        .same_site = "Lax",
        .http_only = true,
        .secure = false
    };
    
    TIME_OPERATION(
        {
            set_cookie(&res, "test_cookie", "test_value", &opts);
            // Cleanup last header
            if (res.header_count > 0) {
                res.header_count--;
                free(res.headers[res.header_count].name);
                free(res.headers[res.header_count].value);
            }
        },
        "Cookie setting"
    );
    
    // Final cleanup
    for (int i = 0; i < res.header_count; i++) {
        free(res.headers[i].name);
        free(res.headers[i].value);
    }
    free(res.headers);
}

void performance_stress_test() {
    printf("\n[STRESS TEST]\n");
    printf("========================================\n");
    
    route_trie_t *trie = route_trie_create();
    void handler(Req *req, Res *res) { (void)req; (void)res; }
    
    // Add many routes
    printf("Adding 1000 routes...\n");
    clock_t start = clock();
    
    for (int i = 0; i < 1000; i++) {
        char path[100];
        snprintf(path, sizeof(path), "/api/v1/endpoint_%d/:id", i);
        route_trie_add(trie, "GET", path, handler, NULL);
    }
    
    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Route addition (1000 routes): %.6f seconds\n", time_taken);
    printf("Routes per second: %.2f\n", 1000.0 / time_taken);
    
    // Test random route lookups
    printf("\nTesting random route lookups...\n");
    
    tokenized_path_t path;
    route_match_t match;
    
    start = clock();
    for (int i = 0; i < 10000; i++) {
        char test_path[100];
        snprintf(test_path, sizeof(test_path), "/api/v1/endpoint_%d/123", i % 1000);
        
        tokenize_path(test_path, &path);
        route_trie_match(trie, "GET", &path, &match);
        free_tokenized_path(&path);
    }
    end = clock();
    
    time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Route lookups (10000): %.6f seconds\n", time_taken);
    printf("Lookups per second: %.2f\n", 10000.0 / time_taken);
    
    route_trie_free(trie);
}

int main() {
    printf("ECEWO Performance Test Suite\n");
    printf("Iterations per test: %d\n", PERFORMANCE_ITERATIONS);
    printf("============================================================\n");
    
    performance_route_trie_operations();
    performance_request_parsing();
    performance_memory_operations();
    performance_middleware_operations();
    performance_cookie_operations();
    performance_stress_test();
    
    printf("\n============================================================\n");
    printf("Performance tests completed.\n");
    printf("NOTE: Results may vary based on system performance.\n");
    return 0;
}
