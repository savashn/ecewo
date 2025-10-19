#include "ecewo.h"
#include "ecewo/cors.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

static struct
{
    const char *origin;
    const char *methods;
    const char *headers;
    const char *credentials;
    const char *max_age;
    bool enabled;
} cors_state = {0};

// =============================================================================
// DEFAULTS
// =============================================================================

static const char *DEFAULT_ORIGIN = "*";
static const char *DEFAULT_METHODS = "GET, POST, PUT, DELETE, PATCH, OPTIONS";
static const char *DEFAULT_HEADERS = "Content-Type";
static const char *DEFAULT_CREDENTIALS = "false";
static const char *DEFAULT_MAX_AGE = "3600";

static void cors_set_defaults(void)
{
    if (!cors_state.origin)
        cors_state.origin = DEFAULT_ORIGIN;
    if (!cors_state.methods)
        cors_state.methods = DEFAULT_METHODS;
    if (!cors_state.headers)
        cors_state.headers = DEFAULT_HEADERS;
    if (!cors_state.credentials)
        cors_state.credentials = DEFAULT_CREDENTIALS;
    if (!cors_state.max_age)
        cors_state.max_age = DEFAULT_MAX_AGE;
}

// =============================================================================
// HELPERS
// =============================================================================

static bool is_origin_allowed(const char *request_origin)
{
    if (!request_origin || !cors_state.origin)
        return false;

    if (strcmp(cors_state.origin, "*") == 0)
        return true;

    return strcmp(request_origin, cors_state.origin) == 0;
}

// =============================================================================
// MIDDLEWARE
// =============================================================================

static int cors_middleware(Req *req, Res *res, Chain *chain)
{
    if (!cors_state.enabled)
    {
        printf("CORS disabled, skipping\n");
        return next(req, res, chain);
    }

    const char *request_origin = get_header(req, "Origin");

    if (req->method && strcmp(req->method, "OPTIONS") == 0)
    {
        if (request_origin && !is_origin_allowed(request_origin))
        {
            send_text(res, 403, "CORS: Origin not allowed");
            return -1;
        }

        if (strcmp(cors_state.origin, "*") == 0)
        {
            set_header(res, "Access-Control-Allow-Origin", "*");
        }
        else if (request_origin)
        {
            set_header(res, "Access-Control-Allow-Origin", request_origin);
        }

        set_header(res, "Access-Control-Allow-Methods", cors_state.methods);
        set_header(res, "Access-Control-Allow-Headers", cors_state.headers);
        set_header(res, "Access-Control-Allow-Credentials", cors_state.credentials);
        set_header(res, "Access-Control-Max-Age", cors_state.max_age);

        reply(res, 204, "text/plain", "", 0);
        return -1;
    }

    bool should_add = false;

    if (strcmp(cors_state.origin, "*") == 0)
    {
        set_header(res, "Access-Control-Allow-Origin", "*");
        should_add = true;
    }
    else if (request_origin && is_origin_allowed(request_origin))
    {
        set_header(res, "Access-Control-Allow-Origin", request_origin);
        should_add = true;
    }

    if (should_add)
    {
        set_header(res, "Access-Control-Allow-Methods", cors_state.methods);
        set_header(res, "Access-Control-Allow-Headers", cors_state.headers);
        set_header(res, "Access-Control-Allow-Credentials", cors_state.credentials);
    }

    return next(req, res, chain);
}

// =============================================================================
// PUBLIC API
// =============================================================================

void cors_init(const Cors *config)
{
    cors_state.enabled = false;

    if (config)
    {
        // Sadece pointer'ları kopyala
        cors_state.origin = config->origin;
        cors_state.methods = config->methods;
        cors_state.headers = config->headers;
        cors_state.credentials = config->credentials;
        cors_state.max_age = config->max_age;
    }
    else
    {
        cors_state.origin = NULL;
        cors_state.methods = NULL;
        cors_state.headers = NULL;
        cors_state.credentials = NULL;
        cors_state.max_age = NULL;
    }

    cors_set_defaults();
    hook(cors_middleware);
    cors_state.enabled = true;
}
