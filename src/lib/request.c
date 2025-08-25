#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "../../vendors/arena.h"
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
    if (new_capacity == 0)
        return -2;

    char *new_buffer = arena_realloc(arena, *buffer, *capacity, new_capacity);
    if (!new_buffer)
    {
        fprintf(stderr, "Arena buffer reallocation failed\n");
        return -1;
    }

    *buffer = new_buffer;
    *capacity = new_capacity;
    return 0;
}

// llhttp callback for URL
static int on_url_cb(llhttp_t *parser, const char *at, size_t length)
{
    if (!parser || !parser->data || !at)
        return 1;

    http_context_t *context = (http_context_t *)parser->data;

    // Check URL length limit
    if (context->url_length + length > MAX_URL_LENGTH)
    {
        fprintf(stderr, "URL too long: %zu bytes\n", context->url_length + length);
        return 1;
    }

    if (ensure_buffer_capacity(context->arena, &context->url, &context->url_capacity,
                               context->url_length, length) != 0)
    {
        return 1;
    }

    arena_memcpy(context->url + context->url_length, at, length);
    context->url_length += length;
    context->url[context->url_length] = '\0';

    return 0;
}

// llhttp callback for headers field
static int on_header_field_cb(llhttp_t *parser, const char *at, size_t length)
{
    if (!parser || !parser->data || !at)
        return 1;

    http_context_t *context = (http_context_t *)parser->data;

    // Check header count limit
    if (context->headers.count >= MAX_HEADERS_COUNT)
    {
        fprintf(stderr, "Too many headers: %d\n", context->headers.count);
        return 1;
    }

    // Check header field size
    if (length > MAX_HEADER_SIZE)
    {
        fprintf(stderr, "Header field too large: %zu bytes\n", length);
        return 1;
    }

    // Reset for new field
    context->header_field_length = 0;

    if (ensure_buffer_capacity(context->arena, &context->current_header_field,
                               &context->header_field_capacity, 0, length) != 0)
    {
        return 1;
    }

    arena_memcpy(context->current_header_field, at, length);
    context->header_field_length = length;
    context->current_header_field[length] = '\0';

    return 0;
}

// Array growth
static int ensure_array_capacity(Arena *arena, request_t *array)
{
    if (!arena || !array)
        return -1;

    if (array->count < array->capacity)
        return 0;

    // Check for overflow
    if (array->capacity > INT_MAX / 2)
        return -1;

    int new_capacity = array->capacity == 0 ? 16 : array->capacity * 2;

    request_item_t *new_items = arena_realloc(arena, array->items,
                                              array->capacity * sizeof(request_item_t),
                                              new_capacity * sizeof(request_item_t));
    if (!new_items)
    {
        fprintf(stderr, "Arena array reallocation failed\n");
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

// llhttp callback for headers value
static int on_header_value_cb(llhttp_t *parser, const char *at, size_t length)
{
    if (!parser || !parser->data || !at)
        return 1;

    http_context_t *context = (http_context_t *)parser->data;

    // Validate header field exists
    if (!context->current_header_field || context->header_field_length == 0)
        return 1;

    // Check header value size
    if (length > MAX_HEADER_SIZE)
    {
        fprintf(stderr, "Header value too large: %zu bytes\n", length);
        return 1;
    }

    if (ensure_array_capacity(context->arena, &context->headers) != 0)
        return 1;

    context->headers.items[context->headers.count].key =
        arena_strdup(context->arena, context->current_header_field);
    if (!context->headers.items[context->headers.count].key)
        return 1;

    char *value = arena_alloc(context->arena, length + 1);
    if (!value)
    {
        return 1;
    }

    arena_memcpy(value, at, length);
    value[length] = '\0';

    context->headers.items[context->headers.count].value = value;
    context->headers.count++;

    // Handle Connection header for keep-alive
    if (context->current_header_field &&
        strcasecmp(context->current_header_field, "Connection") == 0)
    {
        if (length == 10 && strncasecmp(at, "keep-alive", 10) == 0)
        {
            context->keep_alive = 1;
        }
        else if (length == 5 && strncasecmp(at, "close", 5) == 0)
        {
            context->keep_alive = 0;
        }
    }

    return 0;
}

// llhttp callback for method
static int on_method_cb(llhttp_t *parser, const char *at, size_t length)
{
    if (!parser || !parser->data || !at)
        return 1;

    http_context_t *context = (http_context_t *)parser->data;

    // Limit method length
    if (context->method_length + length > MAX_METHOD_LENGTH)
    {
        fprintf(stderr, "Method too long: %zu bytes\n",
                context->method_length + length);
        return 1;
    }

    if (ensure_buffer_capacity(context->arena, &context->method, &context->method_capacity,
                               context->method_length, length) != 0)
    {
        return 1;
    }

    arena_memcpy(context->method + context->method_length, at, length);
    context->method_length += length;
    context->method[context->method_length] = '\0';

    return 0;
}

// llhttp callback for body
static int on_body_cb(llhttp_t *parser, const char *at, size_t length)
{
    if (!parser || !parser->data || !at)
        return 1;

    http_context_t *context = (http_context_t *)parser->data;

    int result = ensure_buffer_capacity(context->arena, &context->body, &context->body_capacity,
                                        context->body_length, length);

    if (result == -2)
    {
        return HPE_USER; // Payload too large
    }
    else if (result != 0)
    {
        return 1;
    }

    arena_memcpy(context->body + context->body_length, at, length);
    context->body_length += length;
    context->body[context->body_length] = '\0';

    return 0;
}

// Callback for HTTP version detection
static int on_version_cb(llhttp_t *parser)
{
    if (!parser || !parser->data)
        return 1;

    http_context_t *context = (http_context_t *)parser->data;

    context->http_major = parser->http_major;
    context->http_minor = parser->http_minor;

    // Set default keep-alive based on HTTP version
    context->keep_alive = (parser->http_major == 1 && parser->http_minor >= 1) ? 1 : 0;

    return 0;
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

    // Initialize buffers
    context->url_capacity = 256;
    context->url = arena_alloc(arena, context->url_capacity);
    if (context->url)
    {
        memset(context->url, 0, context->url_capacity);
    }
    context->url_length = 0;

    context->method_capacity = 16;
    context->method = arena_alloc(arena, context->method_capacity);
    if (context->method)
    {
        memset(context->method, 0, context->method_capacity);
    }
    context->method_length = 0;

    context->header_field_capacity = 64;
    context->current_header_field = arena_alloc(arena, context->header_field_capacity);
    if (context->current_header_field)
    {
        memset(context->current_header_field, 0, context->header_field_capacity);
    }
    context->header_field_length = 0;

    context->body_capacity = 512;
    context->body = arena_alloc(arena, context->body_capacity);
    if (context->body)
    {
        memset(context->body, 0, context->body_capacity);
    }
    context->body_length = 0;

    // Initialize arrays
    context->headers.count = 0;
    context->headers.capacity = 16;
    context->headers.items = arena_alloc(arena, context->headers.capacity * sizeof(request_item_t));
    if (context->headers.items)
    {
        memset(context->headers.items, 0, context->headers.capacity * sizeof(request_item_t));
    }

    // Other fields start empty
    memset(&context->query_params, 0, sizeof(request_t));
    memset(&context->url_params, 0, sizeof(request_t));

    context->keep_alive = 0;
    context->http_major = 1;
    context->http_minor = 0;
}

// Function to clean up HTTP context (arena-aware - just clears pointers)
void http_context_free(http_context_t *context)
{
    if (!context)
        return;

    // Clear pointers - arena handles the memory
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

    memset(context, 0, sizeof(http_context_t));
}

// Query parsing
void parse_query(Arena *arena, const char *query_string, request_t *query)
{
    if (!arena || !query)
        return;

    memset(query, 0, sizeof(request_t));

    if (!query_string || strlen(query_string) == 0)
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
            param_count++;
    }

    // Limit parameters
    if (param_count > MAX_QUERY_PARAMS)
    {
        fprintf(stderr, "Too many query parameters: %d\n", param_count);
        param_count = MAX_QUERY_PARAMS;
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

    char *buffer = arena_alloc(arena, query_len + 1);
    if (!buffer)
    {
        query->items = NULL;
        query->capacity = 0;
        return;
    }

    arena_memcpy(buffer, query_string, query_len);
    buffer[query_len] = '\0';

    char *pair = strtok(buffer, "&");
    while (pair && query->count < query->capacity)
    {
        char *eq = strchr(pair, '=');
        if (eq)
        {
            *eq = '\0';
            query->items[query->count].key = arena_strdup(arena, pair);
            query->items[query->count].value = arena_strdup(arena, eq + 1);

            if (query->items[query->count].key &&
                query->items[query->count].value)
            {
                query->count++;
            }
        }
        pair = strtok(NULL, "&");
    }
}

// Get value by key
const char *get_req(const request_t *request, const char *key)
{
    if (!request || !request->items || !key)
        return NULL;

    for (int i = 0; i < request->count; i++)
    {
        if (request->items[i].key && strcmp(request->items[i].key, key) == 0)
            return request->items[i].value;
    }
    return NULL;
}
