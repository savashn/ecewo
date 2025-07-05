#ifndef REQUEST_H
#define REQUEST_H

#include <stdbool.h>
#include "llhttp.h"
#include "compat.h"

#define INITIAL_CAPACITY 16

typedef struct
{
    char *key;
    char *value;
} request_item_t;

typedef struct
{
    request_item_t *items;
    int count;
    int capacity;
} request_t;

// HTTP parsing context structure to hold state during parsing
typedef struct
{
    llhttp_t parser;            // llhttp parser instance
    llhttp_settings_t settings; // llhttp parser settings

    // Dynamic URL parsing state
    char *url;           // Dynamic URL buffer
    size_t url_length;   // Current URL length
    size_t url_capacity; // URL buffer capacity

    char *method;           // Dynamic method buffer
    size_t method_length;   // Current method length
    size_t method_capacity; // Method buffer capacity

    // Request data containers
    request_t headers;      // Headers container
    request_t query_params; // Query parameters container
    request_t url_params;   // URL parameters container

    // Body parsing
    char *body;           // Request body buffer
    size_t body_length;   // Body length
    size_t body_capacity; // Body buffer capacity

    // Keep-alive tracking
    int keep_alive; // 1 for keep-alive, 0 for close

    // HTTP version
    int http_major; // Major HTTP version
    int http_minor; // Minor HTTP version

    // Temporary header parsing - also dynamic
    char *current_header_field;   // Dynamic current header field buffer
    size_t header_field_length;   // Current header field length
    size_t header_field_capacity; // Header field buffer capacity
} http_context_t;

// Function to initialize the http context
void http_context_init(http_context_t *context);

// Function to cleanup the http context
void http_context_free(http_context_t *context);

// Parse the query string into request_t structure
void parse_query(const char *query_string, request_t *query);

// Parse path parameters based on route pattern
void parse_params(const char *path, const char *route_path, request_t *params);

// Get value by key from request_t structure
const char *get_req(const request_t *request, const char *key);

static inline const char *get_params(const request_t *req, const char *key)
{
    return get_req(&req->params, key);
}
static inline const char *get_query(const request_t *req, const char *key)
{
    return get_req(&req->query, key);
}
static inline const char *get_headers(const request_t *req, const char *key)
{
    return get_req(&req->headers, key);
}

#endif
