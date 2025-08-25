#ifndef ECEWO_COOKIE_H
#define ECEWO_COOKIE_H

#include "../lib/router.h"
#include <stdbool.h>

typedef struct
{
    int max_age;     // Seconds, -1 for session cookie
    char *path;      // Cookie path (default: "/")
    char *domain;    // Cookie domain (optional)
    char *same_site; // "Strict", "Lax", or "None"
    bool http_only;  // Prevents JavaScript access
    bool secure;     // HTTPS only (required for SameSite=None)
} cookie_options_t;

// Get cookie value by name (automatically URL decoded, supports UTF-8)
char *get_cookie(Req *req, const char *name);

// Set cookie with options (automatically URL encoded, supports UTF-8 values)
// Note: Cookie NAMES must be ASCII tokens, cookie VALUES support full UTF-8
void set_cookie(Res *res, const char *name, const char *value, cookie_options_t *options);

#endif
