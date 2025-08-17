#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include "ecewo.h"
#include "router.h"
#include "request.h"
#include "route_trie.h"
#include "middleware.h"
#include "cors.h"
#include "cookie.h"
#include "server.h"

// Test framework macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s - %s\n", __func__, message); \
            return 0; \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("PASS: %s\n", __func__); \
        return 1; \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        printf("Running %s...\n", #test_func); \
        if (test_func()) { \
            tests_passed++; \
        } else { \
            tests_failed++; \
        } \
        total_tests++; \
    } while(0)

// Global test counters
extern int tests_passed;
extern int tests_failed; 
extern int total_tests;

// Test helper counters
extern int middleware_call_count;
extern int handler_call_count;

// Test helper functions
int test_middleware1(Req *req, Res *res, Chain *chain);
int test_middleware2(Req *req, Res *res, Chain *chain);
void test_handler(Req *req, Res *res);

// Helper function to clean up request_t
void cleanup_request_t(request_t *req);

// Test runners for organized execution
void run_unit_tests(void);

#endif
