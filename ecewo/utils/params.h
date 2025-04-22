#ifndef PARAMS_H
#define PARAMS_H

#define MAX_DYNAMIC_PARAMS 10

typedef struct
{
    char *key;
    char *value;
} params_item_t;

typedef struct
{
    params_item_t *params;
    int count;
} params_t;

void parse_dynamic_params(const char *path, const char *route_path, params_t *params);
const char *params_get(params_t *params, const char *key);

#endif
