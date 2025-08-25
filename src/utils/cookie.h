#ifndef ECEWO_COOKIE_H
#define ECEWO_COOKIE_H

#include "../lib/router.h"
#include <stdbool.h>

typedef struct
{
    int max_age;
    char *path;
    char *domain;
    char *same_site; // "Strict", "Lax", "None"
    bool http_only;
    bool secure;
} cookie_options_t;

char *get_cookie(Req *req, const char *name);
void set_cookie(Res *res, const char *name, const char *value, cookie_options_t *options);

#endif
