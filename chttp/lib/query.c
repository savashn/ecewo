#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "query.h"

#define MAX_QUERY_PARAMS 20

void parse_query_string(const char *query_string, query_t *query)
{
    query->count = 0;

    if (!query_string || strlen(query_string) == 0)
        return;

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
            query->items[query->count].key = strdup(pair);
            query->items[query->count].value = strdup(eq + 1);
            query->count++;
        }
        pair = strtok(NULL, "&");
    }
}

const char *query_get(query_t *query, const char *key)
{
    for (int i = 0; i < query->count; i++)
    {
        if (strcmp(query->items[i].key, key) == 0)
        {
            return query->items[i].value;
        }
    }
    return NULL;
}
