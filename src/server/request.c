#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "request.h"

#define MIN_BUFFER_SIZE 64
#define GROWTH_FACTOR 1.5
#define MAX_SINGLE_ALLOCATION (10 * 1024 * 1024)
#define ABSOLUTE_MAX_REQUEST (50 * 1024 * 1024)
#define MAX_HEADER_SIZE (8 * 1024)
#define MAX_URL_LENGTH 2048
#define MAX_METHOD_LENGTH 16
#define MAX_HEADERS_COUNT 100
#define MAX_QUERY_PARAMS 100

// llhttp specific error reasons
#define ERROR_REASON_URL_TOO_LONG "URL exceeds maximum allowed length"
#define ERROR_REASON_HEADER_TOO_LARGE "HTTP header field or value too large"
#define ERROR_REASON_TOO_MANY_HEADERS "Too many HTTP headers"
#define ERROR_REASON_METHOD_TOO_LONG "HTTP method name too long"
#define ERROR_REASON_PAYLOAD_TOO_LARGE "Request payload exceeds maximum size"
#define ERROR_REASON_INVALID_HEADER_FIELD "Invalid or missing header field"
#define ERROR_REASON_MEMORY_ALLOCATION "Memory allocation failed"
#define ERROR_REASON_INVALID_METHOD "Invalid HTTP method"

// C99-compliant character validation helper
static int is_invalid_token_char(unsigned char c)
{
    // RFC 7230 token characters - return 1 if invalid, 0 if valid
    if (c < 33 || c > 126)
        return 1;

    // Check each invalid character explicitly for C99 compliance
    if (c == '(' || c == ')' || c == '<' || c == '>' || c == '@' ||
        c == ',' || c == ';' || c == ':' || c == '\\' || c == '"' ||
        c == '/' || c == '[' || c == ']' || c == '?' || c == '=' ||
        c == '{' || c == '}' || c == ' ' || c == '\t')
    {
        return 1;
    }

    return 0;
}

// Calculate next buffer size for arena allocation
static size_t calculate_next_size(size_t current, size_t needed)
{
    size_t new_size, next;

    if (needed > ABSOLUTE_MAX_REQUEST)
    {
        fprintf(stderr, "Request too large: %zu bytes\n", needed);
        return 0;
    }

    if (needed <= current)
        return current;

    new_size = current < MIN_BUFFER_SIZE ? MIN_BUFFER_SIZE : current;

    while (new_size < needed)
    {
        // Check for overflow before multiplication
        if (new_size > SIZE_MAX / GROWTH_FACTOR)
        {
            new_size = needed + MIN_BUFFER_SIZE;
            break;
        }

        next = (size_t)(new_size * GROWTH_FACTOR);
        if (next <= new_size)
        { // Overflow protection
            new_size = needed + MIN_BUFFER_SIZE;
            break;
        }
        new_size = next;
    }

    // Cap at maximum single allocation
    if (new_size > MAX_SINGLE_ALLOCATION && needed <= MAX_SINGLE_ALLOCATION)
    {
        new_size = MAX_SINGLE_ALLOCATION;
    }

    return new_size > ABSOLUTE_MAX_REQUEST ? ABSOLUTE_MAX_REQUEST : new_size;
}

// Buffer reallocation
static int ensure_buffer_capacity(Arena *arena, char **buffer, size_t *capacity,
                                  size_t current_length, size_t additional_needed)
{
    size_t total_needed, new_capacity;
    char *new_buffer;

    if (!arena || !buffer || !capacity)
        return -1;

    total_needed = current_length + additional_needed + 1;

    if (total_needed <= *capacity)
        return 0; // No reallocation needed

    if (total_needed > ABSOLUTE_MAX_REQUEST)
    {
        fprintf(stderr, "Request exceeds maximum size: %zu bytes\n", total_needed);
        return -2;
    }

    new_capacity = calculate_next_size(*capacity, total_needed);
    if (new_capacity == 0 || new_capacity < total_needed)
        return -2; // Calculation failed or overflow

    new_buffer = arena_realloc(arena, *buffer, *capacity, new_capacity);
    if (!new_buffer)
        return -1; // Memory allocation failed

    *buffer = new_buffer;
    *capacity = new_capacity;
    return 0;
}

// llhttp callback for URL
int on_url_cb(llhttp_t *parser, const char *at, size_t length)
{
    http_context_t *context;
    int result;

    if (!parser || !parser->data || !at || length == 0)
        return HPE_INTERNAL;

    context = (http_context_t *)parser->data;

    // Check URL length limit
    if (context->url_length + length > MAX_URL_LENGTH)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_URL_TOO_LONG);
        return HPE_USER;
    }

    result = ensure_buffer_capacity(context->arena, &context->url, &context->url_capacity,
                                    context->url_length, length);
    if (result == -2)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_URL_TOO_LONG);
        return HPE_USER;
    }
    else if (result != 0)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_MEMORY_ALLOCATION);
        return HPE_INTERNAL;
    }

    // Use memmove for safety in case of overlapping memory
    memmove(context->url + context->url_length, at, length);
    context->url_length += length;
    context->url[context->url_length] = '\0';

    return HPE_OK;
}

// llhttp callback for headers field
int on_header_field_cb(llhttp_t *parser, const char *at, size_t length)
{
    http_context_t *context;
    size_t i;
    int result;

    if (!parser || !parser->data || !at || length == 0)
        return HPE_INTERNAL;

    context = (http_context_t *)parser->data;

    // Check header count limit
    if (context->headers.count >= MAX_HEADERS_COUNT)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_TOO_MANY_HEADERS);
        return HPE_USER;
    }

    // Check header field size
    if (length > MAX_HEADER_SIZE)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_HEADER_TOO_LARGE);
        return HPE_USER;
    }

    // Validate header field characters (RFC 7230 compliant)
    for (i = 0; i < length; i++)
    {
        unsigned char c = (unsigned char)at[i];
        if (is_invalid_token_char(c))
        {
            llhttp_set_error_reason(parser, "Invalid character in header field name");
            return HPE_USER;
        }
    }

    // Reset for new field
    context->header_field_length = 0;

    result = ensure_buffer_capacity(context->arena, &context->current_header_field,
                                    &context->header_field_capacity, 0, length);
    if (result == -2)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_HEADER_TOO_LARGE);
        return HPE_USER;
    }
    else if (result != 0)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_MEMORY_ALLOCATION);
        return HPE_INTERNAL;
    }

    memmove(context->current_header_field, at, length);
    context->header_field_length = length;
    context->current_header_field[length] = '\0';

    return HPE_OK;
}

// Enhanced array capacity management
int ensure_array_capacity(Arena *arena, request_t *array)
{
    int new_capacity, i;
    size_t old_size, new_size;
    request_item_t *new_items;

    if (!arena || !array)
        return -1;

    if (array->count < array->capacity)
        return 0;

    // Check for overflow
    if (array->capacity > MAX_HEADERS_COUNT / 2)
        return -1;

    new_capacity = array->capacity == 0 ? 16 : array->capacity * 2;

    // Cap the maximum capacity
    if (new_capacity > MAX_HEADERS_COUNT)
    {
        new_capacity = MAX_HEADERS_COUNT;
    }

    if (new_capacity <= array->capacity)
    {
        return -1; // Can't grow anymore
    }

    old_size = array->capacity * sizeof(request_item_t);
    new_size = new_capacity * sizeof(request_item_t);

    new_items = arena_realloc(arena, array->items, old_size, new_size);
    if (!new_items)
    {
        return -1;
    }

    // Initialize new elements
    for (i = array->capacity; i < new_capacity; i++)
    {
        new_items[i].key = NULL;
        new_items[i].value = NULL;
    }

    array->items = new_items;
    array->capacity = new_capacity;
    return 0;
}

// Enhanced header value callback
int on_header_value_cb(llhttp_t *parser, const char *at, size_t length)
{
    http_context_t *context;
    char *value;

    if (!parser || !parser->data || !at || length == 0)
        return HPE_INTERNAL;

    context = (http_context_t *)parser->data;

    // Validate header field exists
    if (!context->current_header_field || context->header_field_length == 0)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_INVALID_HEADER_FIELD);
        return HPE_USER;
    }

    // Check header value size
    if (length > MAX_HEADER_SIZE)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_HEADER_TOO_LARGE);
        return HPE_USER;
    }

    // Ensure array capacity
    if (ensure_array_capacity(context->arena, &context->headers) != 0)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_MEMORY_ALLOCATION);
        return HPE_INTERNAL;
    }

    // Store header field
    context->headers.items[context->headers.count].key =
        arena_strdup(context->arena, context->current_header_field);
    if (!context->headers.items[context->headers.count].key)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_MEMORY_ALLOCATION);
        return HPE_INTERNAL;
    }

    // Store header value
    value = arena_alloc(context->arena, length + 1);
    if (!value)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_MEMORY_ALLOCATION);
        return HPE_INTERNAL;
    }

    memmove(value, at, length);
    value[length] = '\0';

    context->headers.items[context->headers.count].value = value;
    context->headers.count++;

    return HPE_OK;
}

// llhttp callback for method
int on_method_cb(llhttp_t *parser, const char *at, size_t length)
{
    http_context_t *context;
    size_t i;
    int result;

    if (!parser || !parser->data || !at || length == 0)
        return HPE_INTERNAL;

    context = (http_context_t *)parser->data;

    // Limit method length
    if (context->method_length + length > MAX_METHOD_LENGTH)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_METHOD_TOO_LONG);
        return HPE_USER;
    }

    // Validate method characters (RFC 7231 compliant)
    for (i = 0; i < length; i++)
    {
        unsigned char c = (unsigned char)at[i];
        if (is_invalid_token_char(c))
        {
            llhttp_set_error_reason(parser, ERROR_REASON_INVALID_METHOD);
            return HPE_USER;
        }
    }

    result = ensure_buffer_capacity(context->arena, &context->method, &context->method_capacity,
                                    context->method_length, length);
    if (result == -2)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_METHOD_TOO_LONG);
        return HPE_USER;
    }
    else if (result != 0)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_MEMORY_ALLOCATION);
        return HPE_INTERNAL;
    }

    memmove(context->method + context->method_length, at, length);
    context->method_length += length;
    context->method[context->method_length] = '\0';

    return HPE_OK;
}

// llhttp callback for body
int on_body_cb(llhttp_t *parser, const char *at, size_t length)
{
    http_context_t *context;
    int result;

    if (!parser || !parser->data || !at || length == 0)
        return HPE_INTERNAL;

    context = (http_context_t *)parser->data;

    result = ensure_buffer_capacity(context->arena, &context->body, &context->body_capacity,
                                    context->body_length, length);

    if (result == -2)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_PAYLOAD_TOO_LARGE);
        return HPE_USER; // Payload too large
    }
    else if (result != 0)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_MEMORY_ALLOCATION);
        return HPE_INTERNAL;
    }

    memmove(context->body + context->body_length, at, length);
    context->body_length += length;
    context->body[context->body_length] = '\0';

    return HPE_OK;
}

// Callback when headers are complete
int on_headers_complete_cb(llhttp_t *parser)
{
    http_context_t *context;

    if (!parser || !parser->data)
        return HPE_INTERNAL;

    context = (http_context_t *)parser->data;

    // Get HTTP version
    context->http_major = llhttp_get_http_major(parser);
    context->http_minor = llhttp_get_http_minor(parser);

    // Determine keep-alive status
    context->keep_alive = llhttp_should_keep_alive(parser);
    context->headers_complete = 1;

    return HPE_OK;
}

// Callback when message is complete
int on_message_complete_cb(llhttp_t *parser)
{
    http_context_t *context;
    char *query_start;

    if (!parser || !parser->data)
        return HPE_INTERNAL;

    context = (http_context_t *)parser->data;
    context->message_complete = 1;

    // Parse query string if URL contains one
    if (context->url && context->url_length > 0)
    {
        query_start = strchr(context->url, '?');
        if (query_start)
        {
            *query_start = '\0'; // Terminate URL part
            parse_query(context->arena, query_start + 1, &context->query_params);
        }
    }

    return HPE_OK;
}

void http_context_init(http_context_t *context,
                       Arena *arena,
                       llhttp_t *reused_parser,
                       llhttp_settings_t *reused_settings)
{
    if (!context || !arena || !reused_parser || !reused_settings)
        return;

    memset(context, 0, sizeof(http_context_t));
    context->arena = arena;

    // Use external parser and settings
    context->parser = reused_parser;
    context->settings = reused_settings;

    // Link parser to context
    context->parser->data = context;

    // Initialize buffers
    context->url_capacity = 512;
    context->url = arena_alloc(arena, context->url_capacity);
    if (context->url)
    {
        context->url[0] = '\0';
    }
    context->url_length = 0;

    context->method_capacity = 32;
    context->method = arena_alloc(arena, context->method_capacity);
    if (context->method)
    {
        context->method[0] = '\0';
    }
    context->method_length = 0;

    context->header_field_capacity = 128;
    context->current_header_field = arena_alloc(arena, context->header_field_capacity);
    if (context->current_header_field)
    {
        context->current_header_field[0] = '\0';
    }
    context->header_field_length = 0;

    context->body_capacity = 1024;
    context->body = arena_alloc(arena, context->body_capacity);
    if (context->body)
    {
        context->body[0] = '\0';
    }
    context->body_length = 0;

    // Initialize arrays
    context->headers.count = 0;
    context->headers.capacity = 32;
    context->headers.items = arena_alloc(arena, context->headers.capacity * sizeof(request_item_t));
    if (context->headers.items)
    {
        memset(context->headers.items, 0, context->headers.capacity * sizeof(request_item_t));
    }

    // Initialize empty containers
    memset(&context->query_params, 0, sizeof(request_t));
    memset(&context->url_params, 0, sizeof(request_t));

    // Default settings
    context->keep_alive = 1;
    context->message_complete = 0;
    context->headers_complete = 0;
    context->last_error = HPE_OK;
    context->error_reason = NULL;
}

// Main parsing function
parse_result_t http_parse_request(http_context_t *context, const char *data, size_t len)
{
    llhttp_errno_t err;

    if (!context || !data || len == 0)
        return PARSE_ERROR;

    err = llhttp_execute(context->parser, data, len);

    // Store error information
    context->last_error = err;
    context->error_reason = llhttp_get_error_reason(context->parser);

    switch (err)
    {
    case HPE_OK:
        if (context->message_complete)
        {
            return PARSE_SUCCESS;
        }
        return PARSE_INCOMPLETE;

    case HPE_PAUSED:
    case HPE_PAUSED_UPGRADE:
        return PARSE_INCOMPLETE;

    case HPE_USER:
        // User-defined errors (size limits, etc.)
        return PARSE_OVERFLOW;

    default:
        return PARSE_ERROR;
    }
}

// Check if message needs EOF to complete
bool http_message_needs_eof(const http_context_t *context)
{
    if (!context)
        return 0;

    return llhttp_message_needs_eof(context->parser) != 0;
}

// Finish parsing when EOF is reached
parse_result_t http_finish_parsing(http_context_t *context)
{
    llhttp_errno_t err;

    if (!context)
        return PARSE_ERROR;

    err = llhttp_finish(context->parser);

    context->last_error = err;
    context->error_reason = llhttp_get_error_reason(context->parser);

    switch (err)
    {
    case HPE_OK:
        return PARSE_SUCCESS;
    case HPE_USER:
        return PARSE_OVERFLOW;
    default:
        return PARSE_ERROR;
    }
}

// Query parsing
void parse_query(Arena *arena, const char *query_string, request_t *query)
{
    size_t query_len;
    int param_count, i;
    char *buffer, *pair, *eq;

    if (!arena || !query)
        return;

    memset(query, 0, sizeof(request_t));

    if (!query_string || *query_string == '\0')
        return;

    query_len = strlen(query_string);
    if (query_len > MAX_URL_LENGTH)
    {
        fprintf(stderr, "Query string too long: %zu bytes\n", query_len);
        return;
    }

    // Count parameters
    param_count = 1;
    for (buffer = (char *)query_string; *buffer; buffer++)
    {
        if (*buffer == '&')
        {
            param_count++;
            if (param_count > MAX_QUERY_PARAMS)
            {
                param_count = MAX_QUERY_PARAMS;
                break;
            }
        }
    }

    query->capacity = param_count;
    query->items = arena_alloc(arena, query->capacity * sizeof(request_item_t));
    if (!query->items)
    {
        query->capacity = 0;
        return;
    }

    // Initialize items
    for (i = 0; i < query->capacity; i++)
    {
        query->items[i].key = NULL;
        query->items[i].value = NULL;
    }

    // Create working copy
    buffer = arena_alloc(arena, query_len + 1);
    if (!buffer)
    {
        query->items = NULL;
        query->capacity = 0;
        return;
    }

    memcpy(buffer, query_string, query_len);
    buffer[query_len] = '\0';

    // Parse parameters
    pair = strtok(buffer, "&");
    while (pair && query->count < query->capacity)
    {
        eq = strchr(pair, '=');
        if (eq)
        {
            *eq = '\0';

            // Only add if both key and value are non-empty
            if (*pair && *(eq + 1))
            {
                query->items[query->count].key = arena_strdup(arena, pair);
                query->items[query->count].value = arena_strdup(arena, eq + 1);

                if (query->items[query->count].key && query->items[query->count].value)
                {
                    query->count++;
                }
            }
        }
        pair = strtok(NULL, "&");
    }
}

// Get value by key
const char *get_req(const request_t *request, const char *key)
{
    size_t key_len;
    uint16_t i;

    if (!request || !request->items || !key || request->count == 0)
        return NULL;

    key_len = strlen(key);

    for (i = 0; i < request->count; i++)
    {
        if (request->items[i].key)
        {
            if (strlen(request->items[i].key) == key_len &&
                strncmp(request->items[i].key, key, key_len) == 0)
            {
                return request->items[i].value;
            }
        }
    }
    return NULL;
}

// Utility function to print request information (debugging)
void print_request_info(const http_context_t *context)
{
    int i;

    if (!context)
        return;

    printf("=== HTTP Request Info ===\n");
    printf("Method: %s\n", context->method ? context->method : "NULL");
    printf("URL: %s\n", context->url ? context->url : "NULL");
    printf("HTTP Version: %d.%d\n", context->http_major, context->http_minor);
    printf("Keep-Alive: %s\n", context->keep_alive ? "true" : "false");
    printf("Message Complete: %s\n", context->message_complete ? "true" : "false");
    printf("Headers Complete: %s\n", context->headers_complete ? "true" : "false");
    printf("Body Length: %zu\n", context->body_length);

    printf("Headers (%d):\n", context->headers.count);
    for (i = 0; i < context->headers.count; i++)
    {
        printf("  %s: %s\n",
               context->headers.items[i].key ? context->headers.items[i].key : "NULL",
               context->headers.items[i].value ? context->headers.items[i].value : "NULL");
    }

    printf("Query Parameters (%d):\n", context->query_params.count);
    for (i = 0; i < context->query_params.count; i++)
    {
        printf("  %s = %s\n",
               context->query_params.items[i].key ? context->query_params.items[i].key : "NULL",
               context->query_params.items[i].value ? context->query_params.items[i].value : "NULL");
    }
    printf("========================\n");
}

// Convert parse result to string
const char *parse_result_to_string(parse_result_t result)
{
    switch (result)
    {
    case PARSE_SUCCESS:
        return "PARSE_SUCCESS";
    case PARSE_INCOMPLETE:
        return "PARSE_INCOMPLETE";
    case PARSE_ERROR:
        return "PARSE_ERROR";
    case PARSE_OVERFLOW:
        return "PARSE_OVERFLOW";
    default:
        return "UNKNOWN";
    }
}
