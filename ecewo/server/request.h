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

    // Current URL parsing state
    char url[512];   // URL buffer
    char method[16]; // Method buffer

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

    // Temporary header parsing
    char current_header_field[128]; // Current header field being parsed
    int header_field_len;           // Length of current header field
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
const char *get_req(request_t *request, const char *key);

// Free memory used by request_t structure
void free_req(request_t *request);

#endif // REQUEST_H
