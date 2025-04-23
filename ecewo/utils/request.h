#ifndef REQUEST_H
#define REQUEST_H

typedef struct
{
    char *key;
    char *value;
} requests_t;

typedef struct
{
    requests_t *items;
    int count;
} request_t;

void parse_query(const char *query_string, request_t *query);
const char *query_get(request_t *query, const char *key);
void free_query(request_t *query);

void parse_params(const char *path, const char *route_path, request_t *params);
const char *params_get(request_t *params, const char *key);
void free_params(request_t *params);

void parse_headers(const char *request, request_t *headers);
const char *headers_get(request_t *headers, const char *key);
void free_headers(request_t *headers);

#endif
