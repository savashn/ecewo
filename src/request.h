#ifndef ECEWO_REQUEST_H
#define ECEWO_REQUEST_H

#include "ecewo.h"
#include "llhttp.h"

// Parse result enumeration
typedef enum
{
    PARSE_SUCCESS = 0,    // Parsing completed successfully
    PARSE_INCOMPLETE = 1, // Need more data
    PARSE_ERROR = -1,     // Parse error occurred
    PARSE_OVERFLOW = -2   // Buffer overflow or size limit exceeded
} parse_result_t;

// HTTP parsing context structure
typedef struct
{
    Arena *arena;                // Arena for this context's memory
    llhttp_t *parser;            // llhttp parser instance
    llhttp_settings_t *settings; // llhttp parser settings

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

    // HTTP protocol information
    uint8_t http_major;   // HTTP major version
    uint8_t http_minor;   // HTTP minor version
    uint16_t status_code; // Status code (for responses)

    // Parsing state flags
    bool message_complete; // true when message parsing is complete
    bool keep_alive;       // true for keep-alive, false for close
    bool headers_complete; // true when headers are fully parsed

    // Temporary header parsing
    char *current_header_field;   // Dynamic current header field buffer
    size_t header_field_length;   // Current header field length
    size_t header_field_capacity; // Header field buffer capacity

    // Error tracking
    llhttp_errno_t last_error; // Last parse error
    const char *error_reason;  // Error description
} http_context_t;

// Using in router.c
parse_result_t http_parse_request(http_context_t *context, const char *data, size_t len);
bool http_message_needs_eof(const http_context_t *context);
parse_result_t http_finish_parsing(http_context_t *context);

// Using in server.c
int on_url_cb(llhttp_t *parser, const char *at, size_t length);
int on_header_field_cb(llhttp_t *parser, const char *at, size_t length);
int on_header_value_cb(llhttp_t *parser, const char *at, size_t length);
int on_method_cb(llhttp_t *parser, const char *at, size_t length);
int on_body_cb(llhttp_t *parser, const char *at, size_t length);
int on_headers_complete_cb(llhttp_t *parser);
int on_message_complete_cb(llhttp_t *parser);

// Utility function for debugging
const char *parse_result_to_string(parse_result_t result);

#endif
