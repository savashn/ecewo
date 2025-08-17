#ifndef MOCK_HELPERS_H
#define MOCK_HELPERS_H

#include "ecewo.h"
#include "request.h"
#include "router.h"

// Mock creation functions
http_context_t* create_mock_http_context(const char* method, const char* url);
Req* create_mock_request(const char* method, const char* path);
Res* create_mock_response(void);

// Mock cleanup functions
void destroy_mock_http_context(http_context_t *ctx);
void destroy_mock_request(Req *req);
void destroy_mock_response(Res *res);

// Helper functions for testing
void add_mock_header(Req *req, const char* name, const char* value);
void add_mock_query_param(Req *req, const char* key, const char* value);
void add_mock_url_param(Req *req, const char* key, const char* value);

// Mock request builder pattern
typedef struct {
    Req *req;
    Res *res;
} mock_context_t;

mock_context_t* create_mock_context(const char* method, const char* path);
void destroy_mock_context(mock_context_t *ctx);

#endif
