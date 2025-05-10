#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "request.h"
#include "compat.h"

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
            // Yeni elemanları NULL’la
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

void parse_headers(const char *request, request_t *headers)
{
    headers->count = 0;
    headers->capacity = INITIAL_CAPACITY;
    headers->items = malloc(sizeof(request_item_t) * headers->capacity);
    if (!headers->items)
    {
        perror("malloc headers");
        headers->capacity = 0;
        return;
    }
    // initialize
    for (int i = 0; i < headers->capacity; i++)
        headers->items[i].key = headers->items[i].value = NULL;

    // find start/end of headers…
    const char *header_start = strstr(request, "\r\n");
    if (!header_start)
        return;
    header_start += 2;
    const char *headers_end = strstr(header_start, "\r\n\r\n");
    if (!headers_end)
        return;

    while (header_start < headers_end)
    {
        const char *header_end = strstr(header_start, "\r\n");
        const char *colon = strchr(header_start, ':');
        if (!header_end)
            break;

        if (colon && colon < header_end)
        {
            // gerekirse büyüt
            if (headers->count >= headers->capacity)
            {
                int new_cap = headers->capacity * 2;
                request_item_t *tmp = realloc(headers->items, sizeof(*tmp) * new_cap);
                if (!tmp)
                {
                    perror("realloc headers");
                    break;
                }
                // yeni elemanları NULL’la
                for (int j = headers->capacity; j < new_cap; j++)
                    tmp[j].key = tmp[j].value = NULL;
                headers->items = tmp;
                headers->capacity = new_cap;
            }

            size_t key_len = colon - header_start;
            size_t value_len = header_end - (colon + 2);
            char *k = malloc(key_len + 1), *v = malloc(value_len + 1);
            if (k && v)
            {
                strncpy(k, header_start, key_len);
                k[key_len] = '\0';
                strncpy(v, colon + 2, value_len);
                v[value_len] = '\0';
                headers->items[headers->count].key = k;
                headers->items[headers->count].value = v;
                headers->count++;
            }
            else
            {
                free(k);
                free(v);
            }
        }
        header_start = header_end + 2;
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
