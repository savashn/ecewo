#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "request.h"

#define INITIAL_CAPACITY 16

#define MIN_BUFFER_SIZE 64
#define GROWTH_FACTOR 1.5
#define MAX_SINGLE_ALLOCATION (10 * 1024 * 1024)
#define ABSOLUTE_MAX_REQUEST (50 * 1024 * 1024)
#define MAX_HEADER_SIZE (8 * 1024)
#define MAX_URL_LENGTH 2048
#define MAX_METHOD_LENGTH 16
#define MAX_HEADERS_COUNT 100
#define MAX_QUERY_PARAMS 100

// Calculate next buffer size
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
static int ensure_buffer_capacity(char **buffer, size_t *capacity,
                                  size_t current_length, size_t additional_needed)
{
    if (!buffer || !capacity)
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

    char *new_buffer = realloc(*buffer, new_capacity);
    if (!new_buffer)
    {
        perror("Buffer reallocation failed");
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

    if (ensure_buffer_capacity(&context->url, &context->url_capacity,
                               context->url_length, length) != 0)
    {
        return 1;
    }

    memcpy(context->url + context->url_length, at, length);
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

    if (ensure_buffer_capacity(&context->current_header_field,
                               &context->header_field_capacity, 0, length) != 0)
    {
        return 1;
    }

    memcpy(context->current_header_field, at, length);
    context->header_field_length = length;
    context->current_header_field[length] = '\0';

    return 0;
}

// Array growth
static int ensure_array_capacity(request_t *array)
{
    if (!array)
        return -1;

    if (array->count < array->capacity)
        return 0;

    // Check for overflow
    if (array->capacity > INT_MAX / 2)
        return -1;

    int new_capacity = array->capacity == 0 ? INITIAL_CAPACITY
                                            : array->capacity * 2;

    request_item_t *new_items = realloc(array->items,
                                        sizeof(request_item_t) * new_capacity);
    if (!new_items)
    {
        perror("Array reallocation failed");
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

    if (ensure_array_capacity(&context->headers) != 0)
        return 1;

    context->headers.items[context->headers.count].key =
        strdup(context->current_header_field);
    if (!context->headers.items[context->headers.count].key)
        return 1;

    char *value = malloc(length + 1);
    if (!value)
    {
        free(context->headers.items[context->headers.count].key);
        context->headers.items[context->headers.count].key = NULL;
        return 1;
    }

    memcpy(value, at, length);
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

    if (ensure_buffer_capacity(&context->method, &context->method_capacity,
                               context->method_length, length) != 0)
    {
        return 1;
    }

    memcpy(context->method + context->method_length, at, length);
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

    int result = ensure_buffer_capacity(&context->body, &context->body_capacity,
                                        context->body_length, length);

    if (result == -2)
    {
        return HPE_USER; // Payload too large
    }
    else if (result != 0)
    {
        return 1;
    }

    memcpy(context->body + context->body_length, at, length);
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

static void free_req(request_t *request)
{
    if (!request || !request->items)
        return;

    for (int i = 0; i < request->count; i++)
    {
        free(request->items[i].key);
        free(request->items[i].value);
    }

    free(request->items);
    request->items = NULL;
    request->count = 0;
    request->capacity = 0;
}

// Initialize HTTP context
void http_context_init(http_context_t *context)
{
    if (!context)
        return;

    memset(context, 0, sizeof(http_context_t));

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
    context->url = calloc(context->url_capacity, 1);
    context->url_length = 0;

    context->method_capacity = 16;
    context->method = calloc(context->method_capacity, 1);
    context->method_length = 0;

    context->header_field_capacity = 64;
    context->current_header_field = calloc(context->header_field_capacity, 1);
    context->header_field_length = 0;

    context->body_capacity = 512;
    context->body = calloc(context->body_capacity, 1);
    context->body_length = 0;

    // Initialize arrays
    context->headers.count = 0;
    context->headers.capacity = 16;
    context->headers.items = calloc(context->headers.capacity,
                                    sizeof(request_item_t));

    // Other fields start empty
    memset(&context->query_params, 0, sizeof(request_t));
    memset(&context->url_params, 0, sizeof(request_t));

    context->keep_alive = 0;
    context->http_major = 1;
    context->http_minor = 0;
}

// Function to clean up HTTP context
void http_context_free(http_context_t *context)
{
    if (!context)
        return;

    free(context->url);
    free(context->method);
    free(context->current_header_field);
    free(context->body);

    free_req(&context->headers);
    free_req(&context->query_params);
    free_req(&context->url_params);

    memset(context, 0, sizeof(http_context_t));
}

void parse_query(const char *query_string, request_t *query)
{
    if (!query)
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
    query->items = calloc(query->capacity, sizeof(request_item_t));
    if (!query->items)
    {
        query->capacity = 0;
        return;
    }

    char *buffer = malloc(query_len + 1);
    if (!buffer)
    {
        free(query->items);
        query->items = NULL;
        query->capacity = 0;
        return;
    }

    strcpy(buffer, query_string);

    char *pair = strtok(buffer, "&");
    while (pair && query->count < query->capacity)
    {
        char *eq = strchr(pair, '=');
        if (eq)
        {
            *eq = '\0';
            query->items[query->count].key = strdup(pair);
            query->items[query->count].value = strdup(eq + 1);

            if (query->items[query->count].key &&
                query->items[query->count].value)
            {
                query->count++;
            }
            else
            {
                free(query->items[query->count].key);
                free(query->items[query->count].value);
            }
        }
        pair = strtok(NULL, "&");
    }

    free(buffer);
}

// Parameter parsing
void parse_params(const char *path, const char *route_path, request_t *params)
{
    if (!params)
        return;

    memset(params, 0, sizeof(request_t));

    if (!path || !route_path)
        return;

    size_t path_len = strlen(path);
    size_t route_len = strlen(route_path);

    if (path_len > MAX_URL_LENGTH || route_len > MAX_URL_LENGTH)
    {
        fprintf(stderr, "Path too long\n");
        return;
    }

    int param_count = 0;
    const char *route_ptr = route_path;

    while (*route_ptr)
    {
        if (*route_ptr == ':')
        {
            route_ptr++;
            while (*route_ptr && *route_ptr != '/')
                route_ptr++;
            param_count++;
        }
        else
        {
            route_ptr++;
        }
    }

    if (param_count == 0)
        return;

    params->capacity = param_count;
    params->items = calloc(params->capacity, sizeof(request_item_t));
    if (!params->items)
    {
        params->capacity = 0;
        return;
    }

    const char *path_start = path;
    const char *route_start = route_path;

    while (*path_start && *route_start && params->count < params->capacity)
    {
        while (*path_start == '/')
            path_start++;
        while (*route_start == '/')
            route_start++;

        if (!*path_start || !*route_start)
            break;

        const char *route_end = route_start;
        while (*route_end && *route_end != '/')
            route_end++;

        const char *path_end = path_start;
        while (*path_end && *path_end != '/')
            path_end++;

        if (*route_start == ':')
        {
            size_t key_len = route_end - route_start - 1;
            size_t value_len = path_end - path_start;

            if (key_len > 0 && value_len > 0)
            {
                char *key = malloc(key_len + 1);
                char *value = malloc(value_len + 1);

                if (key && value)
                {
                    memcpy(key, route_start + 1, key_len);
                    key[key_len] = '\0';

                    memcpy(value, path_start, value_len);
                    value[value_len] = '\0';

                    params->items[params->count].key = key;
                    params->items[params->count].value = value;
                    params->count++;
                }
                else
                {
                    free(key);
                    free(value);
                    break;
                }
            }
        }

        path_start = path_end;
        route_start = route_end;
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
