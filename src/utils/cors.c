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
    cors_t *custom_cors = malloc(sizeof(cors_t));
    if (!custom_cors)
    {
        fprintf(stderr, "Failed to allocate memory for CORS options\n");
        return;
    }

    const char *def_methods = "GET, POST, PUT, DELETE, OPTIONS";
    const char *def_headers = "Content-Type";
    const char *def_credentials = "true";
    const char *def_max_age = "3600";

    custom_cors->origin = opts->origin
                              ? safe_strdup(opts->origin)
                              : NULL;

    custom_cors->methods = opts->methods
                               ? safe_strdup(opts->methods)
                               : safe_strdup(def_methods);

    custom_cors->headers = opts->headers
                               ? safe_strdup(opts->headers)
                               : safe_strdup(def_headers);

    custom_cors->credentials = opts->credentials
                                   ? safe_strdup(opts->credentials)
                                   : safe_strdup(def_credentials);

    custom_cors->max_age = opts->max_age
                               ? safe_strdup(opts->max_age)
                               : safe_strdup(def_max_age);

    custom_cors->enabled = opts->enabled ? opts->enabled : true;
    custom_cors->allow_all_origins = (opts->origin && strcmp(opts->origin, "*") == 0);

    cors_register(custom_cors);
}