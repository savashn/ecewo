#ifndef REQUEST_H
#define REQUEST_H

typedef struct request_item
{
    char *key;
    char *value;
} request_item_t;

typedef struct request
{
    request_item_t *items;
    int count;
} request_t;

void parse_query(const char *query_string, request_t *query);
void parse_params(const char *path, const char *route_path, request_t *params);
void parse_headers(const char *request, request_t *headers);
const char *get_req(request_t *request, const char *key);
void free_req(request_t *request);

#endif
