#ifndef ECEWO_CORS_H
#define ECEWO_CORS_H

#include "../src/lib/router.h"
#include <stdbool.h>

typedef struct
{
    char *origin;
    char *methods;
    char *headers;
    char *credentials;
    char *max_age;
    bool allow_all_origins; // For "*" support
} cors_t;

int cors_cleanup(void);
bool cors_handle_preflight(const http_context_t *ctx, Res *res);
void cors_add_headers(const http_context_t *ctx, Res *res);
int cors_init(cors_t *opts);

#endif
