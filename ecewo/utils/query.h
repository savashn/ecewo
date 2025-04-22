#ifndef QUERY_H
#define QUERY_H

#define MAX_QUERY_PARAMS 20

typedef struct
{
    const char *key;
    const char *value;
} query_item_t;

typedef struct
{
    query_item_t items[MAX_QUERY_PARAMS];
    int count;
} query_t;

void parse_query_string(const char *query_string, query_t *query);
const char *query_get(query_t *query, const char *key);

#endif