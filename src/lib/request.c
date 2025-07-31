#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "request.h"

// Buffer growth strategy
#define MIN_BUFFER_SIZE 64
#define GROWTH_FACTOR 1.5
#define MAX_SINGLE_ALLOCATION (10 * 1024 * 1024) // 10MB limit per allocation
#define ABSOLUTE_MAX_REQUEST (50 * 1024 * 1024)  // 50MB absolute limit

// Calculate next buffer size with optimized growth
static size_t calculate_next_size(size_t current, size_t needed)
{
    if (needed > ABSOLUTE_MAX_REQUEST)
    {
        fprintf(stderr, "Request too large: %zu bytes\n", needed);
        return 0;
    }

    if (needed <= current)
        return current;

    size_t new_size = current;
    if (new_size < MIN_BUFFER_SIZE)
        new_size = MIN_BUFFER_SIZE;

    // Grow by factor until we have enough space
    while (new_size < needed)
    {
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

    return new_size;
}

// Buffer reallocation - only when absolutely necessary
static int ensure_buffer_capacity(char **buffer, size_t *capacity, size_t current_length, size_t additional_needed)
{
    size_t total_needed = current_length + additional_needed + 1; // +1 for null terminator

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
    http_context_t *context = (http_context_t *)parser->data;

    // Only reallocate if necessary
    if (ensure_buffer_capacity(&context->url, &context->url_capacity,
                               context->url_length, length) != 0)
    {
        return 1;
    }

    // Append URL data
    memcpy(context->url + context->url_length, at, length);
    context->url_length += length;
    context->url[context->url_length] = '\0';

    return 0;
}

// llhttp callback for headers field
static int on_header_field_cb(llhttp_t *parser, const char *at, size_t length)
{
    http_context_t *context = (http_context_t *)parser->data;

    // Reset for new field
    context->header_field_length = 0;

    // Only reallocate if necessary
    if (ensure_buffer_capacity(&context->current_header_field, &context->header_field_capacity,
                               0, length) != 0)
    {
        return 1;
    }

    // Copy header field data
    memcpy(context->current_header_field, at, length);
    context->header_field_length = length;
    context->current_header_field[length] = '\0';

    return 0;
}

// Optimized array growth for headers
static int ensure_array_capacity(request_t *array)
{
    if (array->count < array->capacity)
    {
        return 0; // No growth needed
    }

    int new_capacity = array->capacity == 0 ? INITIAL_CAPACITY : (int)(array->capacity * GROWTH_FACTOR);

    request_item_t *new_items = realloc(array->items, sizeof(request_item_t) * new_capacity);
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
    http_context_t *context = (http_context_t *)parser->data;

    // Only grow array if necessary
    if (ensure_array_capacity(&context->headers) != 0)
    {
        return 1;
    }

    // Copy header field and value
    context->headers.items[context->headers.count].key = strdup(context->current_header_field);

    char *value = malloc(length + 1);
    if (!value)
    {
        perror("malloc header value");
        free(context->headers.items[context->headers.count].key);
        return 1;
    }

    memcpy(value, at, length);
    value[length] = '\0';

    context->headers.items[context->headers.count].value = value;
    context->headers.count++;

    // Special handling for Connection header for keep-alive detection
    if (strcasecmp(context->current_header_field, "Connection") == 0)
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
    http_context_t *context = (http_context_t *)parser->data;

    // Only reallocate if necessary
    if (ensure_buffer_capacity(&context->method, &context->method_capacity,
                               context->method_length, length) != 0)
    {
        return 1;
    }

    // Append method data
    memcpy(context->method + context->method_length, at, length);
    context->method_length += length;
    context->method[context->method_length] = '\0';

    return 0;
}

// llhttp callback for body
static int on_body_cb(llhttp_t *parser, const char *at, size_t length)
{
    http_context_t *context = (http_context_t *)parser->data;

    // Only reallocate if necessary
    int result = ensure_buffer_capacity(&context->body, &context->body_capacity,
                                        context->body_length, length);

    if (result == -2)
    {
        return HPE_USER;
    }
    else if (result != 0)
    {
        return 1;
    }

    // Copy body data
    memcpy(context->body + context->body_length, at, length);
    context->body_length += length;
    context->body[context->body_length] = '\0';

    return 0;
}

// Callback for HTTP version detection
static int on_version_cb(llhttp_t *parser)
{
    http_context_t *context = (http_context_t *)parser->data;

    context->http_major = parser->http_major;
    context->http_minor = parser->http_minor;

    // Set default keep-alive based on HTTP version
    if (parser->http_major == 1 && parser->http_minor == 1)
    {
        context->keep_alive = 1; // HTTP/1.1 default is keep-alive
    }
    else
    {
        context->keep_alive = 0; // HTTP/1.0 default is close
    }

    return 0;
}

static void free_req(request_t *request)
{
    if (!request || !request->items)
    {
        return;
    }

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

// Initialization with pre-allocated reasonable sizes
void http_context_init(http_context_t *context)
{
    // Initialize llhttp parser
    llhttp_settings_init(&context->settings);
    llhttp_init(&context->parser, HTTP_REQUEST, &context->settings);

    // Set up callbacks
    context->settings.on_url = on_url_cb;
    context->settings.on_header_field = on_header_field_cb;
    context->settings.on_header_value = on_header_value_cb;
    context->settings.on_method = on_method_cb;
    context->settings.on_body = on_body_cb;
    context->settings.on_headers_complete = on_version_cb;

    context->parser.data = context;

    // Initialize buffers with reasonable starting sizes
    context->url_capacity = 256; // Most URLs are under 256 chars
    context->url = malloc(context->url_capacity);
    context->url_length = 0;
    if (context->url)
    {
        context->url[0] = '\0';
    }
    else
    {
        perror("malloc url");
        context->url_capacity = 0;
    }

    context->method_capacity = 16; // HTTP methods are short
    context->method = malloc(context->method_capacity);
    context->method_length = 0;
    if (context->method)
    {
        context->method[0] = '\0';
    }
    else
    {
        perror("malloc method");
        context->method_capacity = 0;
    }

    context->header_field_capacity = 64; // Most header names are short
    context->current_header_field = malloc(context->header_field_capacity);
    context->header_field_length = 0;
    if (context->current_header_field)
    {
        context->current_header_field[0] = '\0';
    }
    else
    {
        perror("malloc header field");
        context->header_field_capacity = 0;
    }

    context->body_capacity = 512; // Start with reasonable body size
    context->body = malloc(context->body_capacity);
    context->body_length = 0;
    if (context->body)
    {
        context->body[0] = '\0';
    }
    else
    {
        perror("malloc body");
        context->body_capacity = 0;
    }

    // Initialize request structures
    context->headers.count = 0;
    context->headers.capacity = 8; // Most requests have few headers
    context->headers.items = malloc(sizeof(request_item_t) * context->headers.capacity);
    if (context->headers.items)
    {
        for (int i = 0; i < context->headers.capacity; i++)
        {
            context->headers.items[i].key = NULL;
            context->headers.items[i].value = NULL;
        }
    }
    else
    {
        perror("malloc headers");
        context->headers.capacity = 0;
    }

    // Initialize other structures as empty (will allocate when needed)
    context->query_params.count = 0;
    context->query_params.capacity = 0;
    context->query_params.items = NULL;

    context->url_params.count = 0;
    context->url_params.capacity = 0;
    context->url_params.items = NULL;

    // Set default values
    context->keep_alive = 0;
    context->http_major = 1;
    context->http_minor = 0;
}

// Function to clean up HTTP context
void http_context_free(http_context_t *context)
{
    free(context->url);
    free(context->method);
    free(context->current_header_field);
    free(context->body);

    free_req(&context->headers);
    free_req(&context->query_params);
    free_req(&context->url_params);

    // Reset pointers to NULL for safety
    context->url = NULL;
    context->method = NULL;
    context->current_header_field = NULL;
    context->body = NULL;
}

// Query parsing with lazy allocation
void parse_query(const char *query_string, request_t *query)
{
    query->count = 0;
    query->capacity = 0;
    query->items = NULL;

    if (!query_string || strlen(query_string) == 0)
        return;

    // Count parameters first to allocate once
    int param_count = 1;
    for (const char *p = query_string; *p; p++)
    {
        if (*p == '&')
            param_count++;
    }

    query->capacity = param_count;
    query->items = malloc(sizeof(request_item_t) * query->capacity);
    if (!query->items)
    {
        perror("malloc query items");
        query->capacity = 0;
        return;
    }

    // Initialize all items
    for (int i = 0; i < query->capacity; i++)
    {
        query->items[i].key = NULL;
        query->items[i].value = NULL;
    }

    size_t query_len = strlen(query_string);
    char *buffer = malloc(query_len + 1);
    if (!buffer)
    {
        perror("malloc query buffer");
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

            if (!query->items[query->count].key || !query->items[query->count].value)
            {
                printf("Memory allocation failed for query item\n");
                free(query->items[query->count].key);
                free(query->items[query->count].value);
                continue;
            }
            query->count++;
        }
        pair = strtok(NULL, "&");
    }

    free(buffer);
}

// Parameter parsing
void parse_params(const char *path, const char *route_path, request_t *params)
{
    params->count = 0;
    params->capacity = 0;
    params->items = NULL;

    if (!path || !route_path)
    {
        return;
    }

    size_t path_len = strlen(path);
    size_t route_len = strlen(route_path);

    char *path_copy = malloc(path_len + 1);
    char *route_copy = malloc(route_len + 1);

    if (!path_copy || !route_copy)
    {
        free(path_copy);
        free(route_copy);
        return;
    }

    strcpy(path_copy, path);
    strcpy(route_copy, route_path);

    // Count dynamic parameters first
    int param_count = 0;
    char *temp_route = strdup(route_copy);
    if (temp_route)
    {
        char *token = strtok(temp_route, "/");
        while (token)
        {
            if (token[0] == ':')
                param_count++;
            token = strtok(NULL, "/");
        }
        free(temp_route);
    }

    if (param_count == 0)
    {
        free(path_copy);
        free(route_copy);
        return; // No parameters to parse
    }

    // Allocate once for all parameters
    params->capacity = param_count;
    params->items = malloc(sizeof(request_item_t) * params->capacity);
    if (!params->items)
    {
        perror("malloc params");
        params->capacity = 0;
        free(path_copy);
        free(route_copy);
        return;
    }

    for (int i = 0; i < params->capacity; i++)
    {
        params->items[i].key = NULL;
        params->items[i].value = NULL;
    }

    // Count how many segments there are
    int path_segment_count = 0;
    int route_segment_count = 0;

    char *temp_path = strdup(path_copy);
    if (temp_path)
    {
        char *token = strtok(temp_path, "/");
        while (token)
        {
            path_segment_count++;
            token = strtok(NULL, "/");
        }
        free(temp_path);
    }

    temp_route = strdup(route_copy);
    if (temp_route)
    {
        char *token = strtok(temp_route, "/");
        while (token)
        {
            route_segment_count++;
            token = strtok(NULL, "/");
        }
        free(temp_route);
    }

    char **path_segments = malloc(sizeof(char *) * path_segment_count);
    char **route_segments = malloc(sizeof(char *) * route_segment_count);

    if (!path_segments || !route_segments)
    {
        free(path_segments);
        free(route_segments);
        free(path_copy);
        free(route_copy);
        free(params->items);
        params->items = NULL;
        params->capacity = 0;
        return;
    }

    // Parse path segments
    int actual_path_count = 0;
    char *token = strtok(path_copy, "/");
    while (token && actual_path_count < path_segment_count)
    {
        path_segments[actual_path_count++] = strdup(token);
        token = strtok(NULL, "/");
    }

    int actual_route_count = 0;
    token = strtok(route_copy, "/");
    while (token && actual_route_count < route_segment_count)
    {
        route_segments[actual_route_count++] = strdup(token);
        token = strtok(NULL, "/");
    }

    int min_segments = actual_path_count < actual_route_count ? actual_path_count : actual_route_count;

    for (int i = 0; i < min_segments && params->count < params->capacity; i++)
    {
        if (route_segments[i] && route_segments[i][0] == ':')
        {
            char *param_key = strdup(route_segments[i] + 1);
            char *param_value = strdup(path_segments[i]);

            if (param_key && param_value)
            {
                params->items[params->count].key = param_key;
                params->items[params->count].value = param_value;
                params->count++;
            }
            else
            {
                free(param_key);
                free(param_value);
            }
        }
    }

    // Free path segments
    for (int i = 0; i < actual_path_count; i++)
    {
        free(path_segments[i]);
    }
    free(path_segments);

    // Free route segments
    for (int i = 0; i < actual_route_count; i++)
    {
        free(route_segments[i]);
    }
    free(route_segments);

    free(path_copy);
    free(route_copy);
}

const char *get_req(const request_t *request, const char *key)
{
    if (!request || !request->items || !key)
    {
        return NULL;
    }

    for (int i = 0; i < request->count; i++)
    {
        if (request->items[i].key && strcmp(request->items[i].key, key) == 0)
        {
            return request->items[i].value;
        }
    }
    return NULL;
}
