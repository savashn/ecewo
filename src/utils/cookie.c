#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "cookie.h"

char *get_cookie(Req *req, const char *name)
{
    if (!req || !req->arena || !name)
        return NULL;

    const char *cookie_header = get_headers(req, "Cookie");
    if (!cookie_header)
        return NULL;

    size_t name_len = strlen(name);
    const char *pos = cookie_header;

    while (pos)
    {
        // Skip whitespace
        while (*pos && isspace(*pos))
            pos++;

        if (!*pos)
            break;

        // Check if this is our cookie
        if (strncmp(pos, name, name_len) == 0 && pos[name_len] == '=')
        {
            // Found our cookie, extract value
            pos += name_len + 1; // Skip 'name='

            // Find the end of the value (either ';' or end of string)
            const char *end = strchr(pos, ';');
            size_t len = end ? (size_t)(end - pos) : strlen(pos);

            // Trim trailing whitespace
            while (len > 0 && isspace(pos[len - 1]))
                len--;

            // Allocate and copy value
            char *value = arena_alloc(req->arena, len + 1);
            if (!value)
                return NULL;

            arena_memcpy(value, pos, len);
            value[len] = '\0';
            return value;
        }

        // Move to next cookie
        pos = strchr(pos, ';');
        if (pos)
            pos++; // Skip the ';'
    }

    return NULL;
}

void set_cookie(Res *res, const char *name, const char *value, cookie_options_t *options)
{
    if (!res || !res->arena || !name || !value)
    {
        fprintf(stderr, "Invalid parameters for set_cookie\n");
        return;
    }

    if (options && options->max_age < 0)
    {
        fprintf(stderr, "Invalid max_age value\n");
        return;
    }

    int max_age = (options && options->max_age >= 0) ? options->max_age : -1;
    const char *path = (options && options->path) ? options->path : "/";
    const char *domain = (options && options->domain) ? options->domain : NULL;
    const char *same_site = (options && options->same_site) ? options->same_site : NULL;
    bool http_only = options ? options->http_only : false;
    bool secure = options ? options->secure : false;

    if (!path)
        path = "/";

    // Build cookie string
    char *cookie_val = arena_sprintf(res->arena, "%s=%s", name, value);
    if (!cookie_val)
    {
        fprintf(stderr, "Cookie formatting error\n");
        return;
    }

    // Add Max-Age if specified
    if (max_age >= 0)
    {
        char *new_cookie = arena_sprintf(res->arena, "%s; Max-Age=%d", cookie_val, max_age);
        if (!new_cookie)
        {
            fprintf(stderr, "Arena sprintf failed for Max-Age\n");
            return;
        }
        cookie_val = new_cookie;
    }

    // Add Path
    char *new_cookie = arena_sprintf(res->arena, "%s; Path=%s", cookie_val, path);
    if (!new_cookie)
    {
        fprintf(stderr, "Arena sprintf failed for Path\n");
        return;
    }
    cookie_val = new_cookie;

    // Add Domain if specified
    if (domain && strlen(domain) > 0)
    {
        new_cookie = arena_sprintf(res->arena, "%s; Domain=%s", cookie_val, domain);
        if (!new_cookie)
        {
            fprintf(stderr, "Arena sprintf failed for Domain\n");
            return;
        }
        cookie_val = new_cookie;
    }

    // Add SameSite if specified
    if (same_site && strlen(same_site) > 0)
    {
        new_cookie = arena_sprintf(res->arena, "%s; SameSite=%s", cookie_val, same_site);
        if (!new_cookie)
        {
            fprintf(stderr, "Arena sprintf failed for SameSite\n");
            return;
        }
        cookie_val = new_cookie;
    }

    // Add HttpOnly if specified
    if (http_only)
    {
        new_cookie = arena_sprintf(res->arena, "%s; HttpOnly", cookie_val);
        if (!new_cookie)
        {
            fprintf(stderr, "Arena sprintf failed for HttpOnly\n");
            return;
        }
        cookie_val = new_cookie;
    }

    // Add Secure if specified
    if (secure)
    {
        new_cookie = arena_sprintf(res->arena, "%s; Secure", cookie_val);
        if (!new_cookie)
        {
            fprintf(stderr, "Arena sprintf failed for Secure\n");
            return;
        }
        cookie_val = new_cookie;
    }

    set_header(res, "Set-Cookie", cookie_val);
}
