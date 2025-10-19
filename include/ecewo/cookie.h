#ifndef ECEWO_COOKIE_H
#define ECEWO_COOKIE_H

#include <stdbool.h>
#include "ecewo.h"
#include "ecewo-config.h"

#ifndef ECEWO_HAS_COOKIE
#error "Cookie module is not enabled. Build with -DECEWO_COOKIE=ON"
#endif

typedef struct
{
    int max_age;     // Seconds, -1 for session cookie
    char *path;      // Cookie path (default: "/")
    char *domain;    // Cookie domain (optional)
    char *same_site; // "Strict", "Lax", or "None"
    bool http_only;  // Prevents JavaScript access
    bool secure;     // HTTPS only (required for SameSite=None)
} Cookie;

char *cookie_get(Req *req, const char *name);
void cookie_set(Res *res, const char *name, const char *value, Cookie *options);

#endif
