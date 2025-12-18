#ifndef ECEWO_TEST_HANDLERS_H
#define ECEWO_TEST_HANDLERS_H

#include "ecewo.h"

// test-params
void handler_single_param(Req *req, Res *res);
void handler_multi_param(Req *req, Res *res);
void handler_overflow_param(Req *req, Res *res);
int test_single_param(void);
int test_multi_param(void);
int test_param_special_chars(void);
int test_overflow_param(void);

// test-query
void handler_query_params(Req *req, Res *res);
int test_query_multiple(void);
int test_query_empty_value(void);
int test_query_no_params(void);

// test-methods
void handler_method_echo(Req *req, Res *res);
void handler_post_body(Req *req, Res *res);
int test_method_get(void);
int test_method_post(void);
int test_method_put(void);
int test_method_delete(void);
int test_method_patch(void);
int test_post_with_body(void);

// test-middleware
int middleware_first(Req *req, Res *res, Chain *chain);
int middleware_second(Req *req, Res *res, Chain *chain);
int middleware_third(Req *req, Res *res, Chain *chain);
void handler_middleware_order(Req *req, Res *res);
int middleware_abort(Req *req, Res *res, Chain *chain);
void handler_should_not_reach(Req *req, Res *res);
int test_middleware_execution_order(void);
int test_middleware_abort(void);

// test-context
int context_middleware(Req *req, Res *res, Chain *chain);
void context_handler(Req *req, Res *res);
int test_context(void);

// test-response
void handler_json_response(Req *req, Res *res);
void handler_html_response(Req *req, Res *res);
void handler_custom_header(Req *req, Res *res);
void handler_status_codes(Req *req, Res *res);
int test_json_content_type(void);
int test_html_content_type(void);
int test_status_201(void);
int test_status_404(void);
int test_status_500(void);
int test_404_unknown_path(void);
int test_404_wrong_method(void);

// test-headers
void handler_echo_headers(Req *req, Res *res);
int test_request_headers(void);

// test-body
void handler_large_body(Req *req, Res *res);
int test_large_body(void);

// test-redirect
void handler_redirect(Req *req, Res *res);
int test_redirect_301(void);

// test-concurrent-request
void handler_counter(Req *req, Res *res);
int test_sequential_requests(void);

// test-task
void handler_compute(Req *req, Res *res);
int test_spawn_with_response(void);

// test-fire-and-forget
int test_spawn_fire_and_forget(void);
void handler_check_counter(Req *req, Res *res);
void handler_fire_and_forget(Req *req, Res *res);

// test-parallel
int test_spawn_parallel(void);
void handler_parallel(Req *req, Res *res);

// test-blocking
int test_spawn_thread_ids(void);
int test_spawn_not_blocking(void);
int test_sync_blocking(void);
void handler_thread_test(Req *req, Res *res);
void handler_get_main_thread(Req *req, Res *res);
void handler_fast(Req *req, Res *res);
void handler_slow(Req *req, Res *res);

// test-root
void handler_root(Req *req, Res *res);
int test_root_path(void);

#endif
