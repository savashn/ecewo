#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>
#include "ecewo/cookie.h"

// RFC 6265 Cookie size limits
#define MAX_COOKIE_NAME_LEN 256
#define MAX_COOKIE_VALUE_LEN 4096
#define MAX_COOKIE_SIZE 4096
#define MAX_COOKIES_PER_REQUEST 50

// RFC 6265: Valid cookie name characters (RFC 2616 token characters)
// Allowed: VCHAR except separators
// Separators: ( ) < > @ , ; : \ " / [ ] ? = { } SP HT
static bool is_token_char(unsigned char c)
{
    // Check if it's a printable ASCII character (0x21-0x7E)
    if (c < 0x21 || c > 0x7E)
        return false;

    switch (c)
    {
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
    case '{':
    case '}':
    case ' ':
    case '\t':
        return false;
    default:
        return true;
    }
}

static int url_decode_char(char high, char low)
{
    int h = (high >= '0' && high <= '9')   ? high - '0'
            : (high >= 'A' && high <= 'F') ? high - 'A' + 10
            : (high >= 'a' && high <= 'f') ? high - 'a' + 10
                                           : -1;

    int l = (low >= '0' && low <= '9')   ? low - '0'
            : (low >= 'A' && low <= 'F') ? low - 'A' + 10
            : (low >= 'a' && low <= 'f') ? low - 'a' + 10
                                         : -1;

    if (h == -1 || l == -1)
        return -1;
    return (h << 4) | l;
}

static char *url_decode(Arena *arena, const char *src, size_t src_len)
{
    if (!arena || !src)
        return NULL;

    char *decoded = arena_alloc(arena, src_len + 1);
    if (!decoded)
        return NULL;

    size_t i = 0, j = 0;
    while (i < src_len)
    {
        if (src[i] == '%' && i + 2 < src_len)
        {
            int c = url_decode_char(src[i + 1], src[i + 2]);
            if (c >= 0)
            {
                decoded[j++] = (char)c;
                i += 3;
                continue;
            }
        }
        decoded[j++] = src[i++];
    }
    decoded[j] = '\0';
    return decoded;
}

static bool is_valid_cookie_name(const char *name)
{
    if (!name || *name == '\0')
        return false;

    size_t len = strlen(name);
    if (len > MAX_COOKIE_NAME_LEN)
        return false;

    // Cookie name must be a RFC 2616 "token"
    for (size_t i = 0; i < len; i++)
    {
        if (!is_token_char((unsigned char)name[i]))
            return false;
    }
    return true;
}

static void trim_whitespace(const char **start, size_t *len)
{
    if (!start || !*start || !len)
        return;

    while (*len > 0 && isspace((unsigned char)**start))
    {
        (*start)++;
        (*len)--;
    }

    while (*len > 0 && isspace((unsigned char)(*start)[*len - 1]))
    {
        (*len)--;
    }
}

static bool needs_encoding(unsigned char c)
{
    // Encode non-ASCII, control characters, and special cookie characters
    if (c < 0x21 || c > 0x7E)
        return true;

    switch (c)
    {
    case '"':
    case ',':
    case ';':
    case '\\':
    case ' ':
        return true;
    default:
        return false;
    }
}

static char *url_encode_value(Arena *arena, const char *value)
{
    if (!arena || !value)
        return NULL;

    size_t len = strlen(value);
    size_t encoded_len = 0;

    for (size_t i = 0; i < len; i++)
    {
        encoded_len += needs_encoding((unsigned char)value[i]) ? 3 : 1;
    }

    char *encoded = arena_alloc(arena, encoded_len + 1);
    if (!encoded)
        return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)value[i];
        if (needs_encoding(c))
        {
            snprintf(&encoded[j], 4, "%%%02X", c);
            j += 3;
        }
        else
        {
            encoded[j++] = (char)c;
        }
    }
    encoded[j] = '\0';
    return encoded;
}

static char *generate_expires(Arena *arena, int max_age_seconds)
{
    if (!arena || max_age_seconds < 0)
        return NULL;

    time_t now = time(NULL);
    time_t expire_time = now + max_age_seconds;
    struct tm *gmt = gmtime(&expire_time);

    char *expires = arena_alloc(arena, 64);
    if (!expires)
        return NULL;

    strftime(expires, 64, "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return expires;
}

char *cookie_get(Req *req, const char *name)
{
    if (!req || !req->arena || !name)
        return NULL;

    if (!is_valid_cookie_name(name))
    {
        fprintf(stderr, "Invalid cookie name: %s (must be RFC 6265 token)\n", name);
        return NULL;
    }

    const char *cookie_header = get_header(req, "Cookie");
    if (!cookie_header)
        return NULL;

    size_t name_len = strlen(name);
    const char *pos = cookie_header;
    int cookie_count = 0;

    while (pos && cookie_count < MAX_COOKIES_PER_REQUEST)
    {
        // Skip whitespace
        while (*pos && isspace((unsigned char)*pos))
            pos++;

        if (!*pos)
            break;

        cookie_count++;

        const char *cookie_end = strchr(pos, ';');
        size_t cookie_len = cookie_end ? (size_t)(cookie_end - pos) : strlen(pos);

        if (cookie_len > MAX_COOKIE_SIZE)
        {
            fprintf(stderr, "Cookie too large: %zu bytes\n", cookie_len);
            pos = cookie_end ? cookie_end + 1 : NULL;
            continue;
        }

        const char *eq = memchr(pos, '=', cookie_len);
        if (!eq)
        {
            pos = cookie_end ? cookie_end + 1 : NULL;
            continue;
        }

        const char *current_name = pos;
        size_t current_name_len = (size_t)(eq - pos);
        trim_whitespace(&current_name, &current_name_len);

        if (current_name_len == name_len &&
            strncmp(current_name, name, name_len) == 0)
        {

            const char *value_start = eq + 1;
            size_t value_len = cookie_len - (size_t)(eq - pos) - 1;
            trim_whitespace(&value_start, &value_len);

            if (value_len == 0)
            {
                char *result = arena_alloc(req->arena, 1);
                if (result)
                    result[0] = '\0';
                return result;
            }

            if (value_len >= 2 && value_start[0] == '"' && value_start[value_len - 1] == '"')
            {
                value_start++;  // Skip opening quote
                value_len -= 2; // Remove both quotes
            }

            char *decoded = url_decode(req->arena, value_start, value_len);
            return decoded;
        }

        pos = cookie_end ? cookie_end + 1 : NULL;
    }

    if (cookie_count >= MAX_COOKIES_PER_REQUEST)
    {
        fprintf(stderr, "Too many cookies in request\n");
    }

    return NULL;
}

void cookie_set(Res *res, const char *name, const char *value, Cookie *options)
{
    if (!res || !res->arena || !name || !value)
    {
        fprintf(stderr, "Invalid parameters for cookie_set\n");
        return;
    }

    if (!is_valid_cookie_name(name))
    {
        fprintf(stderr, "Invalid cookie name: '%s' (must be RFC 6265 token: !#$%%&'*+-.0-9A-Z^_`a-z|~)\n", name);
        fprintf(stderr, "Valid examples: SessionId, user-token, api_key, csrf.token\n");
        fprintf(stderr, "Invalid examples: session id (space), user@token (@ symbol), [session] (brackets)\n");
        return;
    }

    if (strlen(value) > MAX_COOKIE_VALUE_LEN)
    {
        fprintf(stderr, "Cookie value too large: %zu bytes (max %d)\n",
                strlen(value), MAX_COOKIE_VALUE_LEN);
        return;
    }

    if (options && options->max_age < -1)
    {
        fprintf(stderr, "Invalid max_age value: %d (use -1 for session cookie)\n", options->max_age);
        return;
    }

    int max_age = (options && options->max_age >= 0) ? options->max_age : -1;
    const char *path = (options && options->path) ? options->path : "/";
    const char *domain = (options && options->domain) ? options->domain : NULL;
    const char *same_site = (options && options->same_site) ? options->same_site : NULL;
    bool http_only = options ? options->http_only : false;
    bool secure = options ? options->secure : false;

    char *encoded_value = url_encode_value(res->arena, value);
    if (!encoded_value)
    {
        fprintf(stderr, "Failed to encode cookie value\n");
        return;
    }

    char *cookie_val = arena_sprintf(res->arena, "%s=%s", name, encoded_value);
    if (!cookie_val)
    {
        fprintf(stderr, "Arena sprintf failed for cookie base\n");
        return;
    }

    if (max_age >= 0)
    {
        char *new_cookie = arena_sprintf(res->arena, "%s; Max-Age=%d", cookie_val, max_age);
        if (!new_cookie)
        {
            fprintf(stderr, "Arena sprintf failed for Max-Age\n");
            return;
        }
        cookie_val = new_cookie;

        char *expires = generate_expires(res->arena, max_age);
        if (expires)
        {
            new_cookie = arena_sprintf(res->arena, "%s; Expires=%s", cookie_val, expires);
            if (new_cookie)
            {
                cookie_val = new_cookie;
            }
        }
    }

    char *new_cookie = arena_sprintf(res->arena, "%s; Path=%s", cookie_val, path);
    if (!new_cookie)
    {
        fprintf(stderr, "Arena sprintf failed for Path\n");
        return;
    }
    cookie_val = new_cookie;

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

    if (same_site && strlen(same_site) > 0)
    {
        if (strcmp(same_site, "Strict") == 0 ||
            strcmp(same_site, "Lax") == 0 ||
            strcmp(same_site, "None") == 0)
        {

            new_cookie = arena_sprintf(res->arena, "%s; SameSite=%s", cookie_val, same_site);
            if (!new_cookie)
            {
                fprintf(stderr, "Arena sprintf failed for SameSite\n");
                return;
            }
            cookie_val = new_cookie;
        }
        else
        {
            fprintf(stderr, "Invalid SameSite value: %s (use Strict, Lax, or None)\n", same_site);
            return;
        }
    }

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

    if (same_site && strcmp(same_site, "None") == 0 && !secure)
    {
        fprintf(stderr, "Security Error: SameSite=None requires Secure flag for HTTPS\n");
        return;
    }

    if (strlen(cookie_val) > MAX_COOKIE_SIZE)
    {
        fprintf(stderr, "Final cookie too large: %zu bytes (max %d)\n",
                strlen(cookie_val), MAX_COOKIE_SIZE);
        return;
    }

    set_header(res, "Set-Cookie", cookie_val);
}
