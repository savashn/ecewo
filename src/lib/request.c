#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "request.h"
#include "../../vendors/arena.h"

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

// Calculate next buffer size for arena allocation
static size_t calculate_next_size(size_t current, size_t needed)
{
    if (needed > ABSOLUTE_MAX_REQUEST)
    {
        fprintf(stderr, "Request too large: %zu bytes\n", needed);
        return 0;
    }

    if (needed <= current)
        return current;

    size_t new_size = current < MIN_BUFFER_SIZE ? MIN_BUFFER_SIZE : current;

    while (new_size < needed)
    {
        // Check for overflow before multiplication
        if (new_size > SIZE_MAX / GROWTH_FACTOR)
        {
            new_size = needed + MIN_BUFFER_SIZE;
            break;
        }

        size_t next = (size_t)(new_size * GROWTH_FACTOR);
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
    if (!arena || !buffer || !capacity)
        return -1;

    size_t total_needed = current_length + additional_needed + 1;

    if (total_needed <= *capacity)
        return 0; // No reallocation needed

    if (total_needed > ABSOLUTE_MAX_REQUEST)
    {
        fprintf(stderr, "Request exceeds maximum size: %zu bytes\n", total_needed);
        return -2;
    }

    size_t new_capacity = calculate_next_size(*capacity, total_needed);
    if (new_capacity == 0 || new_capacity < total_needed)
        return -2; // Calculation failed or overflow

    char *new_buffer = arena_realloc(arena, *buffer, *capacity, new_capacity);
    if (!new_buffer)
        return -1; // Memory allocation failed

    *buffer = new_buffer;
    *capacity = new_capacity;
    return 0;
}

// llhttp callback for URL
static int on_url_cb(llhttp_t *parser, const char *at, size_t length)
{
    if (!parser || !parser->data || !at || length == 0)
        return HPE_INTERNAL;

    http_context_t *context = (http_context_t *)parser->data;

    // Check URL length limit
    if (context->url_length + length > MAX_URL_LENGTH)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_URL_TOO_LONG);
        return HPE_USER;
    }

    int result = ensure_buffer_capacity(context->arena, &context->url, &context->url_capacity,
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
static int on_header_field_cb(llhttp_t *parser, const char *at, size_t length)
{
    if (!parser || !parser->data || !at || length == 0)
        return HPE_INTERNAL;

    http_context_t *context = (http_context_t *)parser->data;

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

    // Validate header field characters (basic validation)
    for (size_t i = 0; i < length; i++)
    {
        unsigned char c = (unsigned char)at[i];
        // RFC 7230: field-name token characters
        if (c <= 32 || c >= 127 || c == ':' || c == ' ' || c == '\t')
        {
            llhttp_set_error_reason(parser, "Invalid character in header field name");
            return HPE_USER;
        }
    }

    // Reset for new field
    context->header_field_length = 0;

    int result = ensure_buffer_capacity(context->arena, &context->current_header_field,
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
static int ensure_array_capacity(Arena *arena, request_t *array)
{
    if (!arena || !array)
        return -1;

    if (array->count < array->capacity)
        return 0;

    // Check for overflow
    if (array->capacity > MAX_HEADERS_COUNT / 2)
        return -1;

    int new_capacity = array->capacity == 0 ? 16 : array->capacity * 2;

    // Cap the maximum capacity
    if (new_capacity > MAX_HEADERS_COUNT)
    {
        new_capacity = MAX_HEADERS_COUNT;
    }

    if (new_capacity <= array->capacity)
    {
        return -1; // Can't grow anymore
    }

    size_t old_size = array->capacity * sizeof(request_item_t);
    size_t new_size = new_capacity * sizeof(request_item_t);

    request_item_t *new_items = arena_realloc(arena, array->items, old_size, new_size);
    if (!new_items)
    {
        return -1;
    }

    // Initialize new elements
    for (int i = array->capacity; i < new_capacity; i++)
    {
        new_items[i].key = NULL;
        new_items[i].value = NULL;
    }

    array->items = new_items;
    array->capacity = new_capacity;
    return 0;
}

// Optimized case-insensitive string comparison
static int str_case_ncmp(const char *s1, const char *s2, size_t n)
{
    if (!s1 || !s2)
        return s1 - s2; // Handle NULL pointers

    if (n == 0)
        return 0;

    for (size_t i = 0; i < n && *s1 && *s2; i++, s1++, s2++)
    {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);

        if (c1 != c2)
            return c1 - c2;
    }

    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

// Enhanced header value callback with improved Connection header handling
static int on_header_value_cb(llhttp_t *parser, const char *at, size_t length)
{
    if (!parser || !parser->data || !at || length == 0)
        return HPE_INTERNAL;

    http_context_t *context = (http_context_t *)parser->data;

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
    char *value = arena_alloc(context->arena, length + 1);
    if (!value)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_MEMORY_ALLOCATION);
        return HPE_INTERNAL;
    }

    memmove(value, at, length);
    value[length] = '\0';

    context->headers.items[context->headers.count].value = value;
    context->headers.count++;

    if (context->current_header_field &&
        str_case_ncmp(context->current_header_field, "Connection", 10) == 0)
    {
        if (length == 10 && str_case_ncmp(at, "keep-alive", 10) == 0)
        {
            context->keep_alive = 1;
        }
        else if (length == 5 && str_case_ncmp(at, "close", 5) == 0)
        {
            context->keep_alive = 0;
        }
    }

    return HPE_OK;
}

// llhttp callback for method
static int on_method_cb(llhttp_t *parser, const char *at, size_t length)
{
    if (!parser || !parser->data || !at || length == 0)
        return HPE_INTERNAL;

    http_context_t *context = (http_context_t *)parser->data;

    // Limit method length
    if (context->method_length + length > MAX_METHOD_LENGTH)
    {
        llhttp_set_error_reason(parser, ERROR_REASON_METHOD_TOO_LONG);
        return HPE_USER;
    }

    // Validate method characters (should be uppercase letters)
    for (size_t i = 0; i < length; i++)
    {
        unsigned char c = (unsigned char)at[i];
        if (!isupper(c) && c != '-' && c != '_')
        {
            llhttp_set_error_reason(parser, "Invalid character in HTTP method");
            return HPE_USER;
        }
    }

    int result = ensure_buffer_capacity(context->arena, &context->method, &context->method_capacity,
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
static int on_body_cb(llhttp_t *parser, const char *at, size_t length)
{
    if (!parser || !parser->data || !at || length == 0)
        return HPE_INTERNAL;

    http_context_t *context = (http_context_t *)parser->data;

    int result = ensure_buffer_capacity(context->arena, &context->body, &context->body_capacity,
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

// Callback for HTTP version detection
static int on_version_cb(llhttp_t *parser)
{
    if (!parser || !parser->data)
        return HPE_INTERNAL;

    http_context_t *context = (http_context_t *)parser->data;

    context->http_major = parser->http_major;
    context->http_minor = parser->http_minor;

    // Set default keep-alive based on HTTP version
    context->keep_alive = (parser->http_major == 1 && parser->http_minor >= 1) ? 1 : 0;

    return HPE_OK;
}

// Initialize HTTP context
void http_context_init(http_context_t *context, Arena *arena)
{
    if (!context || !arena)
        return;

    memset(context, 0, sizeof(http_context_t));
    context->arena = arena;

    // Initialize llhttp parser
    llhttp_settings_init(&context->settings);

    // Set up callbacks
    context->settings.on_url = on_url_cb;
    context->settings.on_header_field = on_header_field_cb;
    context->settings.on_header_value = on_header_value_cb;
    context->settings.on_method = on_method_cb;
    context->settings.on_body = on_body_cb;
    context->settings.on_headers_complete = on_version_cb;

    llhttp_init(&context->parser, HTTP_REQUEST, &context->settings);
    context->parser.data = context;

    // Enable stricter parsing
    llhttp_set_lenient_headers(&context->parser, 0);
    llhttp_set_lenient_chunked_length(&context->parser, 0);

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

    // Default HTTP settings
    context->keep_alive = 0;
    context->http_major = 1;
    context->http_minor = 1; // Default to HTTP/1.1
}

// Cleanup HTTP context
void http_context_free(http_context_t *context)
{
    if (!context)
        return;

    // Clear all pointers - arena handles memory deallocation
    context->arena = NULL;
    context->url = NULL;
    context->method = NULL;
    context->current_header_field = NULL;
    context->body = NULL;

    context->headers.items = NULL;
    context->headers.count = 0;
    context->headers.capacity = 0;

    context->query_params.items = NULL;
    context->query_params.count = 0;
    context->query_params.capacity = 0;

    context->url_params.items = NULL;
    context->url_params.count = 0;
    context->url_params.capacity = 0;

    // Zero out the entire structure
    memset(context, 0, sizeof(http_context_t));
}

// Query parsing
void parse_query(Arena *arena, const char *query_string, request_t *query)
{
    if (!arena || !query)
        return;

    memset(query, 0, sizeof(request_t));

    if (!query_string || *query_string == '\0')
        return;

    size_t query_len = strlen(query_string);
    if (query_len > MAX_URL_LENGTH)
    {
        fprintf(stderr, "Query string too long: %zu bytes\n", query_len);
        return;
    }

    // Count parameters
    int param_count = 1;
    for (const char *p = query_string; *p; p++)
    {
        if (*p == '&')
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
    for (int i = 0; i < query->capacity; i++)
    {
        query->items[i].key = NULL;
        query->items[i].value = NULL;
    }

    // Create working copy
    char *buffer = arena_alloc(arena, query_len + 1);
    if (!buffer)
    {
        query->items = NULL;
        query->capacity = 0;
        return;
    }

    memcpy(buffer, query_string, query_len);
    buffer[query_len] = '\0';

    // Parse parameters
    char *pair = strtok(buffer, "&");
    while (pair && query->count < query->capacity)
    {
        char *eq = strchr(pair, '=');
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
    if (!request || !request->items || !key || request->count == 0)
        return NULL;

    size_t key_len = strlen(key);

    for (uint16_t i = 0; i < request->count; i++)
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
