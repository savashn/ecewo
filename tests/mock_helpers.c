#include "mock_helpers.h"
#include <stdlib.h>
#include <string.h>

// Mock HTTP context creation
http_context_t* create_mock_http_context(const char* method, const char* url) {
    http_context_t *ctx = malloc(sizeof(http_context_t));
    if (!ctx) return NULL;
    
    http_context_init(ctx);
    
    if (method && ctx->method_capacity > strlen(method)) {
        strncpy(ctx->method, method, ctx->method_capacity - 1);
        ctx->method[ctx->method_capacity - 1] = '\0';
        ctx->method_length = strlen(method);
    }
    
    if (url && ctx->url_capacity > strlen(url)) {
        strncpy(ctx->url, url, ctx->url_capacity - 1);
        ctx->url[ctx->url_capacity - 1] = '\0';
        ctx->url_length = strlen(url);
    }
    
    return ctx;
}

// Mock request creation
Req* create_mock_request(const char* method, const char* path) {
    Req *req = malloc(sizeof(Req));
    if (!req) return NULL;
    
    memset(req, 0, sizeof(Req));
    
    if (method) req->method = strdup(method);
    if (path) req->path = strdup(path);
    
    // Initialize request_t structures
    req->headers.items = NULL;
    req->headers.count = 0;
    req->headers.capacity = 0;
    
    req->query.items = NULL;
    req->query.count = 0;
    req->query.capacity = 0;
    
    req->params.items = NULL;
    req->params.count = 0;
    req->params.capacity = 0;
    
    return req;
}

// Mock response creation
Res* create_mock_response() {
    Res *res = malloc(sizeof(Res));
    if (!res) return NULL;
    
    memset(res, 0, sizeof(Res));
    res->status = 200;
    res->content_type = "text/plain";
    res->keep_alive = 1;
    res->headers = NULL;
    res->header_count = 0;
    res->header_capacity = 0;
    
    return res;
}

// Helper to add mock header
void add_mock_header(Req *req, const char* name, const char* value) {
    if (!req || !name || !value) return;
    
    if (req->headers.count >= req->headers.capacity) {
        int new_cap = req->headers.capacity ? req->headers.capacity * 2 : 4;
        request_item_t *new_items = realloc(req->headers.items, 
                                           new_cap * sizeof(request_item_t));
        if (!new_items) return;
        
        req->headers.items = new_items;
        req->headers.capacity = new_cap;
    }
    
    req->headers.items[req->headers.count].key = strdup(name);
    req->headers.items[req->headers.count].value = strdup(value);
    req->headers.count++;
}

// Helper to add mock query parameter
void add_mock_query_param(Req *req, const char* key, const char* value) {
    if (!req || !key || !value) return;
    
    if (req->query.count >= req->query.capacity) {
        int new_cap = req->query.capacity ? req->query.capacity * 2 : 4;
        request_item_t *new_items = realloc(req->query.items, 
                                           new_cap * sizeof(request_item_t));
        if (!new_items) return;
        
        req->query.items = new_items;
        req->query.capacity = new_cap;
    }
    
    req->query.items[req->query.count].key = strdup(key);
    req->query.items[req->query.count].value = strdup(value);
    req->query.count++;
}

// Helper to add mock URL parameter
void add_mock_url_param(Req *req, const char* key, const char* value) {
    if (!req || !key || !value) return;
    
    if (req->params.count >= req->params.capacity) {
        int new_cap = req->params.capacity ? req->params.capacity * 2 : 4;
        request_item_t *new_items = realloc(req->params.items, 
                                           new_cap * sizeof(request_item_t));
        if (!new_items) return;
        
        req->params.items = new_items;
        req->params.capacity = new_cap;
    }
    
    req->params.items[req->params.count].key = strdup(key);
    req->params.items[req->params.count].value = strdup(value);
    req->params.count++;
}

// Mock context creation (request + response pair)
mock_context_t* create_mock_context(const char* method, const char* path) {
    mock_context_t *ctx = malloc(sizeof(mock_context_t));
    if (!ctx) return NULL;
    
    ctx->req = create_mock_request(method, path);
    ctx->res = create_mock_response();
    
    if (!ctx->req || !ctx->res) {
        destroy_mock_context(ctx);
        return NULL;
    }
    
    return ctx;
}

// Cleanup functions
void destroy_mock_http_context(http_context_t *ctx) {
    if (ctx) {
        http_context_free(ctx);
        free(ctx);
    }
}

void destroy_mock_request(Req *req) {
    if (req) {
        destroy_req(req);
    }
}

void destroy_mock_response(Res *res) {
    if (res) {
        destroy_res(res);
    }
}

void destroy_mock_context(mock_context_t *ctx) {
    if (ctx) {
        if (ctx->req) destroy_mock_request(ctx->req);
        if (ctx->res) destroy_mock_response(ctx->res);
        free(ctx);
    }
}
