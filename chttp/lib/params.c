#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "params.h"

#define MAX_DYNAMIC_PARAMS 10

void parse_dynamic_params(const char *path, const char *route_path, params_t *params)
{
    printf("Parsing dynamic params for path: %s, route: %s\n", path, route_path);
    params->count = 0;

    const char *param_start = path;
    const char *route_start = route_path;

    while (*route_start != '\0' && *param_start != '\0')
    {
        if (*route_start == ':' && *(route_start + 1) != '\0')
        {
            const char *key_end = strchr(route_start, '/');
            size_t key_len;

            if (key_end)
            {
                key_len = key_end - route_start - 1;
            }
            else
            {
                key_len = strlen(route_start + 1);
            }

            char *param_key = malloc(key_len + 1);
            strncpy(param_key, route_start + 1, key_len);
            param_key[key_len] = '\0';

            const char *param_end = strchr(param_start, '/');
            size_t param_len = (param_end ? param_end - param_start : strlen(param_start));

            char *param_value = malloc(param_len + 1);
            strncpy(param_value, param_start, param_len);
            param_value[param_len] = '\0';

            if (params->count < MAX_DYNAMIC_PARAMS)
            {
                params->params[params->count].key = param_key;
                params->params[params->count].value = param_value;
                params->count++;
            }

            param_start += param_len;
            if (*param_start == '/')
                param_start++;

            route_start = key_end ? key_end : route_start + strlen(route_start);
            if (*route_start == '/')
                route_start++;
        }
        else
        {
            route_start++;
            param_start++;
        }
    }
}

const char *params_get(params_t *params, const char *key)
{
    for (int i = 0; i < params->count; i++)
    {
        if (strcmp(params->params[i].key, key) == 0)
        {
            return params->params[i].value;
        }
    }
    return NULL;
}
