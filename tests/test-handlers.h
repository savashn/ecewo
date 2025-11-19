#ifndef ECEWO_TEST_HANDLERS_H
#define ECEWO_TEST_HANDLERS_H

#include "ecewo.h"

// test-basics
void handler_plaintext(Req *req, Res *res);
int test_plaintext(void);
void handler_json(Req *req, Res *res);
int test_json(void);
void handler_html(Req *req, Res *res);
int test_html(void);

// test-request
void handler_params(Req *req, Res *res);
int test_params(void);
void handler_query(Req *req, Res *res);
int test_query(void);
void handler_headers(Req *req, Res *res);
int test_headers(void);

// test-context
int context_middleware(Req *req, Res *res, Chain *chain);
void context_handler(Req *req, Res *res);
int test_context(void);

// blocking/non-blocking
void instant_handler(Req *req, Res *res);
int slow_middleware(Req *req, Res *res, Chain *chain);
void slow_async_handler(Req *req, Res *res);
void fast_sync_handler(Req *req, Res *res);
int test_not_blocked(void);
int test_sync_blocks(void);

#endif
