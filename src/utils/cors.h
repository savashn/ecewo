#ifndef CORS_H
#define CORS_H

#include "router.h"
#include <stdbool.h>

typedef struct
{
    char *origin;
    char *methods;
    char *headers;
    char *credentials;
    char *max_age;
    bool enabled;
    bool allow_all_origins; // For "*" support
} cors_t;

void cors_register(cors_t *opts);
void reset_cors(void);
bool cors_handle_preflight(const http_context_t *ctx, Res *res);
void cors_add_headers(const http_context_t *ctx, Res *res);
void init_cors(cors_t *opts);
bool is_origin_allowed(const char *origin);

#endif // CORS_H
