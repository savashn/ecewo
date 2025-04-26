#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "request.h"

#define MAX_HEADERS 50
#define MAX_DYNAMIC_PARAMS 20
#define MAX_QUERY_PARAMS 20

void parse_query(const char *query_string, request_t *query)
{
    query->count = 0;

    if (!query_string || strlen(query_string) == 0)
        return;

    query->items = malloc(sizeof(request_t) * MAX_QUERY_PARAMS);
    if (query->items == NULL)
    {
        printf("Memory allocation failed for query items\n");
        return;
    }

    char buffer[1024];
    strncpy(buffer, query_string, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *pair = strtok(buffer, "&");

    while (pair && query->count < MAX_QUERY_PARAMS)
    {
        char *eq = strchr(pair, '=');
        if (eq)
        {
            *eq = '\0';
            query->items[query->count].key = strdup(pair);     // Copy the key
            query->items[query->count].value = strdup(eq + 1); // Copy the value
            query->count++;
        }
        pair = strtok(NULL, "&");
    }
}

void parse_params(const char *path, const char *route_path, request_t *params)
{
    printf("Parsing dynamic params for path: %s, route: %s\n", path, route_path);
    params->count = 0;

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
        if (route_segments[i][0] == ':') // Dynamic parameter
        {
            char *param_key = strdup(route_segments[i] + 1); // Remove the ":" from the key
            char *param_value = strdup(path_segments[i]);    // Get the corresponding value from path

            if (params->count < MAX_DYNAMIC_PARAMS)
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
    headers->items = malloc(sizeof(request_t) * 20);
    headers->count = 0;

    const char *header_start = request;
    const char *header_end;
    const char *colon_pos;

    while ((header_end = strstr(header_start, "\r\n")) != NULL)
    {
        colon_pos = strchr(header_start, ':');
        if (colon_pos && colon_pos < header_end)
        {
            int key_len = colon_pos - header_start;
            int value_len = header_end - colon_pos - 2;

            // Copy the header key and value
            headers->items[headers->count].key = malloc(key_len + 1);
            strncpy(headers->items[headers->count].key, header_start, key_len);
            headers->items[headers->count].key[key_len] = '\0';

            headers->items[headers->count].value = malloc(value_len + 1);
            strncpy(headers->items[headers->count].value, colon_pos + 2, value_len);
            headers->items[headers->count].value[value_len] = '\0';

            headers->count++;
        }

        header_start = header_end + 2; // Move to the next header
        if (headers->count >= 20)      // Reallocate memory if the header count exceeds the limit
        {
            headers->items = realloc(headers->items, sizeof(request_t) * (headers->count + 20));
        }
    }
}

const char *get_req(request_t *request, const char *key)
{
    for (int i = 0; i < request->count; i++)
    {
        if (strcmp(request->items[i].key, key) == 0)
        {
            return request->items[i].value;
        }
    }
    return NULL;
}

void free_req(request_t *request)
{
    for (int i = 0; i < request->count; i++)
    {
        free(request->items[i].key);
        free(request->items[i].value);
    }
    free(request->items);
    request->count = 0;
}
