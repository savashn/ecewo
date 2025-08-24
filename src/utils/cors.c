#include "cors.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static cors_t *g_cors_opts = NULL;

// Helper function to safe-copy the string
static char *safe_strdup(const char *str)
{
    if (!str)
        return NULL;
    return strdup(str);
}

void cors_register(cors_t *opts)
{
    g_cors_opts = opts;
}

void reset_cors(void)
{
    if (g_cors_opts)
    {
        free(g_cors_opts->origin);
        free(g_cors_opts->methods);
        free(g_cors_opts->headers);
        free(g_cors_opts->credentials);
        free(g_cors_opts->max_age);
        free(g_cors_opts);
        g_cors_opts = NULL;
    }
}

static bool is_origin_allowed(const char *origin)
{
    if (!g_cors_opts || !origin)
        return false;

    // Allow all the origins
    if (g_cors_opts->allow_all_origins)
        return true;

    // Allow specific origins
    if (g_cors_opts->origin && strcmp(origin, g_cors_opts->origin) == 0)
    {
        return true;
    }

    return false;
}

bool cors_handle_preflight(const http_context_t *ctx, Res *res)
{
    if (!g_cors_opts || !g_cors_opts->enabled)
        return false;
    if (strcmp(ctx->method, "OPTIONS") != 0)
        return false;

    // Check origin
    const char *origin = get_req(&ctx->headers, "Origin");

    if (origin && !is_origin_allowed(origin))
    {
        res->status = 403;
        return true;
    }

    // Configure preflight response
    res->status = 204;
    res->body = NULL;
    res->body_len = 0;
    res->content_type = "";

    // Add CORS headers
    if (g_cors_opts->allow_all_origins)
    {
        set_header(res, "Access-Control-Allow-Origin", "*");
    }
    else if (origin && is_origin_allowed(origin))
    {
        set_header(res, "Access-Control-Allow-Origin", origin);
    }

    if (g_cors_opts->methods)
        set_header(res, "Access-Control-Allow-Methods", g_cors_opts->methods);
    if (g_cors_opts->headers)
        set_header(res, "Access-Control-Allow-Headers", g_cors_opts->headers);
    if (g_cors_opts->credentials)
        set_header(res, "Access-Control-Allow-Credentials", g_cors_opts->credentials);
    if (g_cors_opts->max_age)
        set_header(res, "Access-Control-Max-Age", g_cors_opts->max_age);

    return true;
}

void cors_add_headers(const http_context_t *ctx, Res *res)
{
    if (!g_cors_opts || !g_cors_opts->enabled)
        return;

    const char *origin = get_req(&ctx->headers, "Origin");

    bool should_add_cors = false;

    // Check origin and add headers
    if (g_cors_opts->allow_all_origins)
    {
        set_header(res, "Access-Control-Allow-Origin", "*");
        should_add_cors = true;
    }
    else if (origin && is_origin_allowed(origin))
    {
        set_header(res, "Access-Control-Allow-Origin", origin);
        should_add_cors = true;
    }

    // Add other headers for allowed origins only
    if (should_add_cors)
    {
        if (g_cors_opts->methods)
            set_header(res, "Access-Control-Allow-Methods", g_cors_opts->methods);
        if (g_cors_opts->headers)
            set_header(res, "Access-Control-Allow-Headers", g_cors_opts->headers);
        if (g_cors_opts->credentials)
            set_header(res, "Access-Control-Allow-Credentials", g_cors_opts->credentials);
    }
}

void init_cors(cors_t *opts)
{
    cors_t *custom_cors = calloc(1, sizeof(cors_t));
    if (!custom_cors)
    {
        fprintf(stderr, "Failed to allocate memory for CORS options\n");
        return;
    }

    static const char *def_methods = "GET, POST, PUT, DELETE, OPTIONS";
    static const char *def_headers = "Content-Type";
    static const char *def_credentials = "true";
    static const char *def_max_age = "3600";

    if (opts && opts->origin)
    {
        custom_cors->origin = safe_strdup(opts->origin);
        if (!custom_cors->origin)
            goto error_cleanup;

        // Check for wildcard
        custom_cors->allow_all_origins = (strcmp(opts->origin, "*") == 0);
    }

    // Methods
    const char *methods_src = (opts && opts->methods) ? opts->methods : def_methods;
    custom_cors->methods = safe_strdup(methods_src);
    if (!custom_cors->methods)
        goto error_cleanup;

    // Headers
    const char *headers_src = (opts && opts->headers) ? opts->headers : def_headers;
    custom_cors->headers = safe_strdup(headers_src);
    if (!custom_cors->headers)
        goto error_cleanup;

    // Credentials
    const char *credentials_src = (opts && opts->credentials) ? opts->credentials : def_credentials;
    custom_cors->credentials = safe_strdup(credentials_src);
    if (!custom_cors->credentials)
        goto error_cleanup;

    // Max age
    const char *max_age_src = (opts && opts->max_age) ? opts->max_age : def_max_age;
    custom_cors->max_age = safe_strdup(max_age_src);
    if (!custom_cors->max_age)
        goto error_cleanup;

    // Enabled flag - default true
    custom_cors->enabled = (opts && opts->enabled != 0) ? opts->enabled : true;

    cors_register(custom_cors);
    return;

error_cleanup:
    // Partial cleanup on allocation failure
    if (custom_cors)
    {
        free(custom_cors->origin);
        free(custom_cors->methods);
        free(custom_cors->headers);
        free(custom_cors->credentials);
        free(custom_cors->max_age);
        free(custom_cors);
    }
    fprintf(stderr, "Failed to allocate CORS strings\n");
}
