#include "ecewo.h"
#include "ecewo/helmet.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

static struct
{
    const char *csp;
    const char *hsts_max_age;
    bool hsts_subdomains;
    bool hsts_preload;
    const char *frame_options;
    const char *referrer_policy;
    const char *xss_protection;
    bool nosniff;
    bool ie_no_open;
    bool enabled;
} helmet_state = {0};

// =============================================================================
// DEFAULTS (moderate security)
// =============================================================================

static const char *DEFAULT_CSP = NULL;
static const char *DEFAULT_HSTS_MAX_AGE = "31536000"; // 1 year
static const char *DEFAULT_FRAME_OPTIONS = "SAMEORIGIN";
static const char *DEFAULT_REFERRER_POLICY = "strict-origin-when-cross-origin";
static const char *DEFAULT_XSS_PROTECTION = "1; mode=block";

static void helmet_set_defaults(void)
{
    if (!helmet_state.csp)
        helmet_state.csp = DEFAULT_CSP;
    if (!helmet_state.hsts_max_age)
        helmet_state.hsts_max_age = DEFAULT_HSTS_MAX_AGE;
    if (!helmet_state.frame_options)
        helmet_state.frame_options = DEFAULT_FRAME_OPTIONS;
    if (!helmet_state.referrer_policy)
        helmet_state.referrer_policy = DEFAULT_REFERRER_POLICY;
    if (!helmet_state.xss_protection)
        helmet_state.xss_protection = DEFAULT_XSS_PROTECTION;

    // nosniff and ie_no_open are true by default
}

// =============================================================================
// MIDDLEWARE
// =============================================================================

static int helmet_middleware(Req *req, Res *res, Chain *chain)
{
    (void)req;

    if (!helmet_state.enabled)
        return next(req, res, chain);

    if (helmet_state.csp)
        set_header(res, "Content-Security-Policy", helmet_state.csp);

    if (helmet_state.hsts_max_age)
    {
        char *hsts = ecewo_sprintf(res, "max-age=%s", helmet_state.hsts_max_age);

        if (helmet_state.hsts_subdomains)
        {
            hsts = ecewo_sprintf(res, "%s; includeSubDomains", hsts);
        }

        if (helmet_state.hsts_preload)
        {
            hsts = ecewo_sprintf(res, "%s; preload", hsts);
        }

        set_header(res, "Strict-Transport-Security", hsts);
    }

    if (helmet_state.frame_options)
        set_header(res, "X-Frame-Options", helmet_state.frame_options);

    if (helmet_state.nosniff)
        set_header(res, "X-Content-Type-Options", "nosniff");

    if (helmet_state.xss_protection)
        set_header(res, "X-XSS-Protection", helmet_state.xss_protection);

    if (helmet_state.referrer_policy)
        set_header(res, "Referrer-Policy", helmet_state.referrer_policy);

    if (helmet_state.ie_no_open)
        set_header(res, "X-Download-Options", "noopen");

    return next(req, res, chain);
}

// =============================================================================
// PUBLIC API
// =============================================================================

void helmet_init(const Helmet *config)
{
    helmet_state.enabled = false;

    if (config)
    {
        helmet_state.csp = config->csp;
        helmet_state.hsts_max_age = config->hsts_max_age;
        helmet_state.hsts_subdomains = config->hsts_subdomains;
        helmet_state.hsts_preload = config->hsts_preload;
        helmet_state.frame_options = config->frame_options;
        helmet_state.referrer_policy = config->referrer_policy;
        helmet_state.xss_protection = config->xss_protection;
        helmet_state.nosniff = config->nosniff;
        helmet_state.ie_no_open = config->ie_no_open;
    }
    else
    {
        helmet_state.csp = NULL;
        helmet_state.hsts_max_age = NULL;
        helmet_state.hsts_subdomains = false;
        helmet_state.hsts_preload = false;
        helmet_state.frame_options = NULL;
        helmet_state.referrer_policy = NULL;
        helmet_state.xss_protection = NULL;
        helmet_state.nosniff = true;
        helmet_state.ie_no_open = true;
    }

    helmet_set_defaults();
    hook(helmet_middleware);
    helmet_state.enabled = true;
}
