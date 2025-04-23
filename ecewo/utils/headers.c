#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "request.h"

#define MAX_HEADERS 50

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

            headers->items[headers->count].key = malloc(key_len + 1);
            strncpy(headers->items[headers->count].key, header_start, key_len);
            headers->items[headers->count].key[key_len] = '\0';

            headers->items[headers->count].value = malloc(value_len + 1);
            strncpy(headers->items[headers->count].value, colon_pos + 2, value_len);
            headers->items[headers->count].value[value_len] = '\0';

            headers->count++;
        }

        header_start = header_end + 2;
        if (headers->count >= 20)
        {
            headers->items = realloc(headers->items, sizeof(request_t) * (headers->count + 20));
        }
    }
}

const char *headers_get(request_t *headers, const char *key)
{
    for (int i = 0; i < headers->count; i++)
    {
        if (strcmp(headers->items[i].key, key) == 0)
        {
            return headers->items[i].value;
        }
    }
    return NULL;
}

void free_headers(request_t *headers)
{
    for (int i = 0; i < headers->count; i++)
    {
        free(headers->items[i].key);
        free(headers->items[i].value);
    }
    free(headers->items);
    headers->count = 0;
}
