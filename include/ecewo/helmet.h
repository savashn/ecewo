#ifndef ECEWO_HELMET_H
#define ECEWO_HELMET_H

#include <stdbool.h>
#include "ecewo-config.h"

#ifndef ECEWO_HAS_HELMET
#error "Helmet module is not enabled. Build with -DECEWO_HELMET=ON"
#endif

typedef struct
{
    // Content Security Policy
    const char *csp;

    // HSTS (HTTP Strict Transport Security)
    const char *hsts_max_age; // e.g., "31536000" (1 year)
    bool hsts_subdomains;
    bool hsts_preload;

    // Other Security Headers
    const char *frame_options;   // "DENY", "SAMEORIGIN"
    const char *referrer_policy; // e.g., "strict-origin-when-cross-origin"
    const char *xss_protection;  // "0" or "1; mode=block"

    // Flags
    bool nosniff;    // X-Content-Type-Options: nosniff
    bool ie_no_open; // X-Download-Options: noopen
} Helmet;

void helmet_init(const Helmet *config);

#endif
