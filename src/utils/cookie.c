#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cookie.h"

char *handle_get_cookie(request_t *headers, const char *name)
{
    const char *cookie_header = get_req(headers, "Cookie");
    if (!cookie_header)
        return NULL;

    // Find the start of the cookie name in the header
    const char *start = strstr(cookie_header, name);
    if (!start || start[strlen(name)] != '=')
        return NULL;

    // Move past 'name=' to the beginning of the value
    start += strlen(name) + 1;

    // Find the end of the value (either ';' or end of string)
    const char *end = strchr(start, ';');
    size_t len = end ? (size_t)(end - start) : strlen(start);

    // Allocate buffer large enough to hold the value plus null terminator
    char *value = malloc(len + 1);
    if (!value)
        return NULL;

    // Copy the cookie value and null-terminate
    memcpy(value, start, len);
    value[len] = '\0';
    return value;
}

void handle_set_cookie(Res *res, const char *name, const char *value, int max_age)
{
    if (!res || !name || !value || max_age < 0)
        return;

    int needed = snprintf(
        NULL, 0,
        "%s=%s; Max-Age=%d; Path=/; HttpOnly; Secure; SameSite=Lax",
        name, value, max_age);
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
    snprintf(cookie_val,
             (size_t)needed + 1,
             "%s=%s; Max-Age=%d; Path=/; HttpOnly; Secure; SameSite=Lax",
             name, value, max_age);

    set_header(res, "Set-Cookie", cookie_val);

    free(cookie_val);
}
