#ifndef REQUEST_H
#define REQUEST_H

#include <stdbool.h>
#include <stddef.h>
#include "llhttp.h"
#include "compat.h"
#include "arena.h"

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
    Arena *arena;               // Arena for this context's memory
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

    // Temporary header parsing
    char *current_header_field;   // Dynamic current header field buffer
    size_t header_field_length;   // Current header field length
    size_t header_field_capacity; // Header field buffer capacity
} http_context_t;

// Function to initialize the http context
void http_context_init(http_context_t *context, Arena *arena);

// Function to cleanup the http context
void http_context_free(http_context_t *context);

// Parse the query string into request_t structure
void parse_query(Arena *arena, const char *query_string, request_t *query);

// Get value by key from request_t structure
const char *get_req(const request_t *request, const char *key);

#endif
