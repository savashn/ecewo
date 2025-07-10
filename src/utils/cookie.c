#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "cookie.h"

char *get_cookie(Req *req, const char *name)
{
    if (!req || !name)
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
            char *value = malloc(len + 1);
            if (!value)
                return NULL;

            memcpy(value, pos, len);
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
    if (!res || !name || !value)
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
    const char *same_site = (options && options->same_site) ? options->same_site : NULL;
    bool http_only = options ? options->http_only : false;
    bool secure = options ? options->secure : false;

    if (!path)
        path = "/";

    int needed = snprintf(NULL, 0, "%s=%s", name, value);

    if (max_age >= 0)
    {
        needed += snprintf(NULL, 0, "; Max-Age=%d", max_age);
    }

    needed += snprintf(NULL, 0, "; Path=%s", path);

    if (same_site && strlen(same_site) > 0)
    {
        needed += snprintf(NULL, 0, "; SameSite=%s", same_site);
    }

    if (http_only)
    {
        needed += strlen("; HttpOnly");
    }

    if (secure)
    {
        needed += strlen("; Secure");
    }

    if (needed < 0)
    {
        fprintf(stderr, "Cookie formatting error\n");
        return;
    }

    char *cookie_val = malloc((size_t)needed + 1);
    if (!cookie_val)
    {
        perror("malloc for cookie_val");
        return;
    }

    int written = snprintf(cookie_val, (size_t)needed + 1, "%s=%s", name, value);
    if (written < 0)
    {
        fprintf(stderr, "Cookie formatting error\n");
        free(cookie_val);
        return;
    }

    if (max_age >= 0)
    {
        int ret = snprintf(cookie_val + written, (size_t)needed + 1 - written, "; Max-Age=%d", max_age);
        if (ret < 0)
            goto error;
        written += ret;
    }

    int ret = snprintf(cookie_val + written, (size_t)needed + 1 - written, "; Path=%s", path);
    if (ret < 0)
        goto error;
    written += ret;

    if (same_site && strlen(same_site) > 0)
    {
        ret = snprintf(cookie_val + written, (size_t)needed + 1 - written, "; SameSite=%s", same_site);
        if (ret < 0)
            goto error;
        written += ret;
    }

    if (http_only)
    {
        ret = snprintf(cookie_val + written, (size_t)needed + 1 - written, "; HttpOnly");
        if (ret < 0)
            goto error;
        written += ret;
    }

    if (secure)
    {
        ret = snprintf(cookie_val + written, (size_t)needed + 1 - written, "; Secure");
        if (ret < 0)
            goto error;
        written += ret;
    }

    set_header(res, "Set-Cookie", cookie_val);
    free(cookie_val);
    return;

error:
    fprintf(stderr, "Cookie formatting error during construction\n");
    free(cookie_val);
}
