#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "request.h"

// llhttp callback for URL
static int on_url_cb(llhttp_t *parser, const char *at, size_t length)
{
    http_context_t *context = (http_context_t *)parser->data;

    // Ensure we don't overflow the buffer
    size_t copy_len = length;
    if (copy_len >= sizeof(context->url))
        copy_len = sizeof(context->url) - 1;

    // Copy URL to context
    memcpy(context->url, at, copy_len);
    context->url[copy_len] = '\0';

    return 0;
}

// llhttp callback for headers field
static int on_header_field_cb(llhttp_t *parser, const char *at, size_t length)
{
    http_context_t *context = (http_context_t *)parser->data;

    // Ensure we don't overflow the buffer
    size_t copy_len = length;
    if (copy_len >= sizeof(context->current_header_field))
        copy_len = sizeof(context->current_header_field) - 1;

    // Copy header field to context
    memcpy(context->current_header_field, at, copy_len);
    context->current_header_field[copy_len] = '\0';
    context->header_field_len = copy_len;

    return 0;
}

// llhttp callback for headers value
static int on_header_value_cb(llhttp_t *parser, const char *at, size_t length)
{
    http_context_t *context = (http_context_t *)parser->data;

    // Check if we've reached capacity
    if (context->headers.count >= context->headers.capacity)
    {
        int new_cap = context->headers.capacity * 2;
        request_item_t *tmp = realloc(context->headers.items, sizeof(*tmp) * new_cap);
        if (!tmp)
        {
            perror("realloc headers");
            return 1; // Error
        }

        // Initialize new elements to NULL
        for (int j = context->headers.capacity; j < new_cap; j++)
        {
            tmp[j].key = tmp[j].value = NULL;
        }

        context->headers.items = tmp;
        context->headers.capacity = new_cap;
    }

    // Copy header field and value
    context->headers.items[context->headers.count].key = strdup(context->current_header_field);

    char *value = malloc(length + 1);
    if (!value)
    {
        perror("malloc header value");
        free(context->headers.items[context->headers.count].key);
        return 1; // Error
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

    // Ensure we don't overflow the buffer
    size_t copy_len = length;
    if (copy_len >= sizeof(context->method))
        copy_len = sizeof(context->method) - 1;

    // Copy method to context
    memcpy(context->method, at, copy_len);
    context->method[copy_len] = '\0';

    return 0;
}

// llhttp callback for body
static int on_body_cb(llhttp_t *parser, const char *at, size_t length)
{
    http_context_t *context = (http_context_t *)parser->data;

    // Ensure we have enough capacity
    if (context->body_length + length > context->body_capacity)
    {
        size_t new_capacity = context->body_capacity * 2;
        if (new_capacity < context->body_length + length)
            new_capacity = context->body_length + length + 1024; // Ensure enough space

        char *new_body = realloc(context->body, new_capacity);
        if (!new_body)
        {
            perror("realloc body");
            return 1; // Error
        }

        context->body = new_body;
        context->body_capacity = new_capacity;
    }

    // Copy body data
    memcpy(context->body + context->body_length, at, length);
    context->body_length += length;
    context->body[context->body_length] = '\0'; // Null-terminate

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
        // HTTP/1.1 default is keep-alive
        context->keep_alive = 1;
    }
    else
    {
        // HTTP/1.0 default is close
        context->keep_alive = 0;
    }

    return 0;
}

// Function to initialize HTTP context
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

    // Set parser data to point to our context
    context->parser.data = context;

    // Initialize other fields
    context->url[0] = '\0';
    context->method[0] = '\0';
    context->current_header_field[0] = '\0';
    context->header_field_len = 0;

    // Initialize body
    context->body = malloc(1024); // Start with 1KB capacity
    if (context->body)
    {
        context->body[0] = '\0';
        context->body_length = 0;
        context->body_capacity = 1024;
    }
    else
    {
        perror("malloc body");
        context->body_capacity = 0;
    }

    // Initialize request structures
    context->headers.count = 0;
    context->headers.capacity = INITIAL_CAPACITY;
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

    context->query_params.count = 0;
    context->query_params.capacity = 0;
    context->query_params.items = NULL;

    context->url_params.count = 0;
    context->url_params.capacity = 0;
    context->url_params.items = NULL;

    // Set default keep-alive to 0 (will be updated during parsing)
    context->keep_alive = 0;

    // Set default HTTP version
    context->http_major = 1;
    context->http_minor = 0;
}

// Function to clean up HTTP context
void http_context_free(http_context_t *context)
{
    // Free body
    if (context->body)
    {
        free(context->body);
        context->body = NULL;
    }

    // Free headers
    free_req(&context->headers);

    // Free query parameters if they were parsed
    free_req(&context->query_params);

    // Free URL parameters if they were parsed
    free_req(&context->url_params);
}

void parse_query(const char *query_string, request_t *query)
{
    query->count = 0;
    query->capacity = INITIAL_CAPACITY;
    query->items = NULL;

    if (!query_string || strlen(query_string) == 0)
        return;

    query->items = malloc(sizeof(request_item_t) * query->capacity);
    if (!query->items)
    {
        perror("malloc");
        query->capacity = 0;
        return;
    }

    // Initialize all items to NULL to prevent double-free issues
    for (int i = 0; i < query->capacity; i++)
    {
        query->items[i].key = NULL;
        query->items[i].value = NULL;
    }

    char buffer[1024];
    strncpy(buffer, query_string, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *pair = strtok(buffer, "&");

    while (pair && query->count < query->capacity)
    {
        char *eq = strchr(pair, '=');
        if (eq)
        {
            *eq = '\0';
            query->items[query->count].key = strdup(pair);     // Copy the key
            query->items[query->count].value = strdup(eq + 1); // Copy the value

            // Check if allocation succeeded
            if (!query->items[query->count].key || !query->items[query->count].value)
            {
                printf("Memory allocation failed for query item\n");
                // Cleanup allocated memory
                if (query->items[query->count].key)
                    free(query->items[query->count].key);
                if (query->items[query->count].value)
                    free(query->items[query->count].value);
                continue;
            }

            query->count++;
        }
        pair = strtok(NULL, "&");
    }
}

void parse_params(const char *path, const char *route_path, request_t *params)
{
    printf("Parsing dynamic params for path: %s, route: %s\n", path, route_path);
    params->count = 0;
    params->capacity = INITIAL_CAPACITY;

    params->items = malloc(sizeof(request_item_t) * params->capacity);
    if (!params->items)
    {
        perror("malloc");
        params->capacity = 0;
        return;
    }

    // Initialize items to ensure we can safely free later
    for (int i = 0; i < params->capacity; i++)
    {
        params->items[i].key = NULL;
        params->items[i].value = NULL;
    }

    char path_copy[256];
    char route_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    strncpy(route_copy, route_path, sizeof(route_copy) - 1);
    route_copy[sizeof(route_copy) - 1] = '\0';

    char *path_segments[20];
    char *route_segments[20];
    int path_segment_count = 0;
    int route_segment_count = 0;

    // Split the path into segments based on "/"
    char *token = strtok(path_copy, "/");
    while (token != NULL && path_segment_count < 20)
    {
        path_segments[path_segment_count++] = token;
        token = strtok(NULL, "/");
    }

    // Split the route path into segments based on "/"
    token = strtok(route_copy, "/");
    while (token != NULL && route_segment_count < 20)
    {
        route_segments[route_segment_count++] = token;
        token = strtok(NULL, "/");
    }

    // Check if the number of segments in the path matches the route
    if (path_segment_count != route_segment_count)
    {
        printf("Warning: Path and route segment counts differ (%d vs %d)\n",
               path_segment_count, route_segment_count);
    }

    int min_segments = path_segment_count < route_segment_count ? path_segment_count : route_segment_count;

    // Parse dynamic parameters (indicated by ":") and store them in the params object
    for (int i = 0; i < min_segments; i++)
    {
        if (params->count >= params->capacity)
        {
            int new_cap = params->capacity * 2;
            request_item_t *tmp = realloc(params->items, sizeof(*tmp) * new_cap);
            if (!tmp)
            {
                perror("realloc");
                break;
            }
            // Initialize new elements to NULL
            for (int j = params->capacity; j < new_cap; j++)
            {
                tmp[j].key = tmp[j].value = NULL;
            }
            params->items = tmp;
            params->capacity = new_cap;
        }

        if (route_segments[i][0] == ':') // Dynamic parameter
        {
            char *param_key = strdup(route_segments[i] + 1); // Remove the ":" from the key
            char *param_value = strdup(path_segments[i]);    // Get the corresponding value from path

            if (!param_key || !param_value)
            {
                printf("Memory allocation failed for param key or value\n");
                if (param_key)
                    free(param_key);
                if (param_value)
                    free(param_value);
                continue;
            }

            if (params->count < params->capacity)
            {
                params->items[params->count].key = param_key;
                params->items[params->count].value = param_value;
                params->count++;
                printf("Parsed parameter: %s = %s\n", param_key, param_value);
            }
            else
            {
                free(param_key);
                free(param_value);
                printf("Error: Maximum parameter count reached\n");
                break;
            }
        }
        else if (strcmp(route_segments[i], path_segments[i]) != 0) // Static segment mismatch
        {
            printf("Warning: Static segment mismatch at position %d: '%s' vs '%s'\n",
                   i, route_segments[i], path_segments[i]);
        }
    }
}

const char *get_req(request_t *request, const char *key)
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

void free_req(request_t *request)
{
    if (!request || !request->items)
    {
        return;
    }

    for (int i = 0; i < request->count; i++)
    {
        if (request->items[i].key)
        {
            free(request->items[i].key);
            request->items[i].key = NULL;
        }

        if (request->items[i].value)
        {
            free(request->items[i].value);
            request->items[i].value = NULL;
        }
    }

    free(request->items);
    request->items = NULL;
    request->count = 0;
}
