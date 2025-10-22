#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include "ecewo/session.h"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

static Session *sessions = NULL;
static int max_sessions = 0;
static int initialized = 0;

#define KV_DELIMITER '\x1F'        // ASCII Unit Separator - separates key from value
#define PAIR_DELIMITER '\x1E'      // ASCII Record Separator - separates key-value pairs
#define MAX_SESSION_DATA_SIZE 4096

static const Cookie SESSION_COOKIE_DEFAULTS = {
    .max_age = 3600, // 1 hour default
    .path = "/",
    .domain = NULL,
    .same_site = "Lax",
    .http_only = true,
    .secure = false,
};

static int is_url_safe(char c)
{
    return (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~');
}

static char *url_encode(const char *str)
{
    if (!str)
        return NULL;

    size_t len = strlen(str);
    size_t encoded_len = len * 3 + 1; // worst case: every char becomes %XX
    char *encoded = malloc(encoded_len);
    if (!encoded)
        return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len && j < encoded_len - 3; i++)
    {
        if (is_url_safe(str[i]))
        {
            encoded[j++] = str[i];
        }
        else
        {
            int written = snprintf(encoded + j, encoded_len - j, "%%%02X", (unsigned char)str[i]);
            if (written < 0 || written >= (int)(encoded_len - j))
            {
                free(encoded);
                return NULL;
            }
            j += written;
        }
    }
    encoded[j] = '\0';
    return encoded;
}

static int hex_to_int(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

static char *url_decode(const char *str)
{
    if (!str)
        return NULL;

    size_t len = strlen(str);
    char *decoded = malloc(len + 1);
    if (!decoded)
        return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (str[i] == '%' && i + 2 < len)
        {
            int high = hex_to_int(str[i + 1]);
            int low = hex_to_int(str[i + 2]);
            if (high >= 0 && low >= 0)
            {
                decoded[j++] = (char)(high * 16 + low);
                i += 2;
            }
            else
            {
                decoded[j++] = str[i];
            }
        }
        else
        {
            decoded[j++] = str[i];
        }
    }
    decoded[j] = '\0';
    return decoded;
}

static char *safe_strdup(const char *str)
{
    if (!str)
        return NULL;
    size_t len = strlen(str);
    char *copy = malloc(len + 1);
    if (copy)
    {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

int session_init(void)
{
    if (initialized)
        return 1;

    const int initial_capacity = MAX_SESSIONS_DEFAULT;
    sessions = calloc(initial_capacity, sizeof(Session));
    if (!sessions)
        return 0;

    max_sessions = initial_capacity;
    initialized = 1;
    return 1;
}

void session_cleanup(void)
{
    if (!initialized)
        return;

    for (int i = 0; i < max_sessions; i++)
    {
        if (sessions[i].id[0] != '\0' && sessions[i].data != NULL)
        {
            free(sessions[i].data);
            sessions[i].data = NULL;
        }
    }

    free(sessions);
    sessions = NULL;
    max_sessions = 0;
    initialized = 0;
}

static int resize_sessions(int new_capacity)
{
    if (new_capacity <= max_sessions)
        return 1;

    Session *new_sessions = realloc(sessions, new_capacity * sizeof(Session));
    if (!new_sessions)
        return 0;

    for (int i = max_sessions; i < new_capacity; i++)
    {
        memset(&new_sessions[i], 0, sizeof(Session));
    }

    sessions = new_sessions;
    max_sessions = new_capacity;
    return 1;
}

static int get_random_bytes(unsigned char *buffer, size_t length)
{
#ifdef _WIN32
    HCRYPTPROV hCryptProv;
    int result = 0;

    if (CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        if (CryptGenRandom(hCryptProv, (DWORD)length, buffer))
            result = 1;
        CryptReleaseContext(hCryptProv, 0);
    }
    return result;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return 0;

    size_t bytes_read = 0;
    while (bytes_read < length)
    {
        ssize_t result = read(fd, buffer + bytes_read, length - bytes_read);
        if (result < 0)
        {
            if (errno == EINTR)
                continue;
            close(fd);
            return 0;
        }
        bytes_read += (size_t)result;
    }

    close(fd);
    return 1;
#endif
}

static void generate_session_id(char *buffer)
{
    unsigned char entropy[SESSION_ID_LEN];

    if (!get_random_bytes(entropy, SESSION_ID_LEN))
    {
        fprintf(stderr, "Random generation failed, using fallback method\n");

        unsigned int seed = (unsigned int)time(NULL);
#ifdef _WIN32
        seed ^= (unsigned int)GetCurrentProcessId();
#else
        seed ^= (unsigned int)getpid();
#endif

        static unsigned int counter = 0;
        seed ^= ++counter;

        void *stack_var = &seed;
        seed ^= ((uintptr_t)stack_var >> 3);

        srand(seed);
        for (size_t i = 0; i < SESSION_ID_LEN; i++)
        {
            entropy[i] = (unsigned char)(rand() & 0xFF);
        }
    }

    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    const size_t charset_len = sizeof(charset) - 1;

    for (size_t i = 0; i < SESSION_ID_LEN; i++)
    {
        buffer[i] = charset[entropy[i] % charset_len];
    }

    // Clear entropy from memory
    memset(entropy, 0, SESSION_ID_LEN);
    buffer[SESSION_ID_LEN] = '\0';
}

static void cleanup_expired_sessions(void)
{
    time_t now = time(NULL);
    for (int i = 0; i < max_sessions; i++)
    {
        if (sessions[i].id[0] != '\0' && sessions[i].expires < now)
            session_free(&sessions[i]);
    }
}

Session *session_create(int max_age)
{
    if (!initialized && !session_init())
        return NULL;

    cleanup_expired_sessions();

    int slot = -1;
    for (int i = 0; i < max_sessions; i++)
    {
        if (sessions[i].id[0] == '\0')
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        if (!resize_sessions(max_sessions * 2))
            return NULL;
        slot = max_sessions / 2;
    }

    generate_session_id(sessions[slot].id);
    sessions[slot].expires = time(NULL) + max_age;

    free(sessions[slot].data);
    sessions[slot].data = safe_strdup("");
    if (!sessions[slot].data)
    {
        sessions[slot].id[0] = '\0';
        return NULL;
    }

    return &sessions[slot];
}

Session *session_find(const char *id)
{
    if (!id || !initialized)
        return NULL;

    time_t now = time(NULL);
    for (int i = 0; i < max_sessions; i++)
    {
        if (sessions[i].id[0] != '\0' &&
            strcmp(sessions[i].id, id) == 0 &&
            sessions[i].expires >= now)
        {
            return &sessions[i];
        }
    }

    return NULL;
}

static void remove_key_from_data(char *data, const char *encoded_key)
{
    if (!data || !encoded_key)
        return;

    char *search_start = data;

    while (search_start && *search_start)
    {
        char *key_end = strchr(search_start, KV_DELIMITER);
        if (!key_end)
            break;

        size_t key_len = key_end - search_start;

        if (key_len == strlen(encoded_key) &&
            strncmp(search_start, encoded_key, key_len) == 0)
        {
            char *value_start = key_end + 1;
            char *pair_end = strchr(value_start, PAIR_DELIMITER);

            if (pair_end)
            {
                char *after_pair = pair_end + 1;
                size_t remaining_len = strlen(after_pair);
                memmove(search_start, after_pair, remaining_len + 1);
            }
            else
            {
                *search_start = '\0';
            }
            break;
        }

        char *next_pair = strchr(search_start, PAIR_DELIMITER);
        search_start = next_pair ? next_pair + 1 : NULL;
    }
}

void session_value_set(Session *sess, const char *key, const char *value)
{
    if (!sess || !key || !value)
        return;

    char *encoded_key = url_encode(key);
    char *encoded_value = url_encode(value);
    if (!encoded_key || !encoded_value)
    {
        if (encoded_key)
            free(encoded_key);
        else if (encoded_value)
            free(encoded_value);
        return;
    }

    size_t current_len = sess->data ? strlen(sess->data) : 0;
    size_t key_len = strlen(encoded_key);
    size_t value_len = strlen(encoded_value);
    size_t additional_size = key_len + value_len + 2; // KV_DELIMITER + PAIR_DELIMITER

    if (current_len + additional_size > MAX_SESSION_DATA_SIZE)
    {
        fprintf(stderr, "Session data size limit exceeded\n");
        free(encoded_key);
        free(encoded_value);
        return;
    }

    if (sess->data && strlen(sess->data) > 0)
        remove_key_from_data(sess->data, encoded_key);

    current_len = sess->data ? strlen(sess->data) : 0;
    size_t new_len = current_len + key_len + value_len + 3; // key + KV_DELIM + value + PAIR_DELIM + null

    char *new_data = malloc(new_len);
    if (new_data)
    {
        if (current_len > 0)
        {
            strcpy(new_data, sess->data);
        }
        else
        {
            new_data[0] = '\0';
        }

        strcat(new_data, encoded_key);

        size_t pos = strlen(new_data);
        new_data[pos] = KV_DELIMITER;
        new_data[pos + 1] = '\0';

        strcat(new_data, encoded_value);

        pos = strlen(new_data);
        new_data[pos] = PAIR_DELIMITER;
        new_data[pos + 1] = '\0';

        free(sess->data);
        sess->data = new_data;
    }

    free(encoded_key);
    free(encoded_value);
}

char *session_value_get(Session *sess, const char *key)
{
    if (!sess || !key || !sess->data || strlen(sess->data) == 0)
        return NULL;

    char *encoded_key = url_encode(key);
    if (!encoded_key)
        return NULL;

    char *search_start = sess->data;
    char *result = NULL;

    while (search_start && *search_start)
    {
        char *key_end = strchr(search_start, KV_DELIMITER);
        if (!key_end)
            break;

        size_t key_len = key_end - search_start;

        if (key_len == strlen(encoded_key) &&
            strncmp(search_start, encoded_key, key_len) == 0)
        {
            char *value_start = key_end + 1;
            char *value_end = strchr(value_start, PAIR_DELIMITER);

            size_t value_len = value_end ? (size_t)(value_end - value_start) : strlen(value_start);

            char *encoded_value = malloc(value_len + 1);
            if (encoded_value)
            {
                strncpy(encoded_value, value_start, value_len);
                encoded_value[value_len] = '\0';

                result = url_decode(encoded_value);
                free(encoded_value);
            }
            break;
        }

        char *next_pair = strchr(search_start, PAIR_DELIMITER);
        search_start = next_pair ? next_pair + 1 : NULL;
    }

    free(encoded_key);
    return result;
}

void session_value_remove(Session *sess, const char *key)
{
    if (!sess || !key || !sess->data)
        return;

    char *encoded_key = url_encode(key);
    if (!encoded_key)
        return;

    remove_key_from_data(sess->data, encoded_key);
    free(encoded_key);
}

void session_free(Session *sess)
{
    if (!sess)
        return;

    memset(sess->id, 0, sizeof(sess->id));
    sess->expires = 0;
    if (sess->data)
    {
        free(sess->data);
        sess->data = NULL;
    }
}

Session *session_get(Req *req)
{
    char *sid = cookie_get(req, "session");
    if (!sid)
        return NULL;

    Session *sess = session_find(sid);
    // Note: cookie_get returns arena-allocated memory, so no need to free sid
    return sess;
}

void session_print_all(void)
{
    time_t now = time(NULL);
    printf("=== Sessions ===\n");

    for (int i = 0; i < max_sessions; i++)
    {
        Session *s = &sessions[i];
        if (s->id[0] == '\0')
            continue;

        printf("[#%02d] id=%.8s..., expires in %lds\n",
               i, s->id, (long)(s->expires - now));

        if (s->data && strlen(s->data) > 0)
        {
            char *data_copy = safe_strdup(s->data);
            if (data_copy)
            {
                char *pair_start = data_copy;
                int pair_count = 0;

                while (pair_start && *pair_start)
                {
                    char *key_end = strchr(pair_start, KV_DELIMITER);
                    if (!key_end)
                        break;

                    *key_end = '\0';
                    char *value_start = key_end + 1;
                    char *pair_end = strchr(value_start, PAIR_DELIMITER);

                    if (pair_end)
                        *pair_end = '\0';

                    char *decoded_key = url_decode(pair_start);
                    char *decoded_value = url_decode(value_start);

                    printf("      %s = %s\n",
                           decoded_key ? decoded_key : pair_start,
                           decoded_value ? decoded_value : value_start);

                    free(decoded_key);
                    free(decoded_value);

                    pair_start = pair_end ? pair_end + 1 : NULL;
                    pair_count++;
                }

                if (pair_count == 0)
                    printf("      (empty)\n");

                free(data_copy);
            }
        }
        else
        {
            printf("      (empty)\n");
        }
    }
    printf("================\n");
}

void session_send(Res *res, Session *sess, Cookie *options)
{
    if (!res || !sess || sess->id[0] == '\0')
    {
        fprintf(stderr, "Error on session sending\n");
        return;
    }

    time_t now = time(NULL);
    int max_age = (int)difftime(sess->expires, now);
    if (max_age < 0)
        return;

    Cookie opts = options ? *options : SESSION_COOKIE_DEFAULTS;
    opts.max_age = max_age;

    cookie_set(res, "session", sess->id, &opts);
}

void session_destroy(Res *res, Session *sess, Cookie *options)
{
    if (!res || !sess || sess->id[0] == '\0')
        return;

    Cookie opts = options ? *options : SESSION_COOKIE_DEFAULTS;
    opts.max_age = 0; // Expire immediately

    cookie_set(res, "session", "", &opts);
    session_free(sess);
}
