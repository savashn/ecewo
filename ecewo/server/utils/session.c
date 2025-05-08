#include <stdlib.h>
#include <string.h>

#include "session.h"
#include "request.h"
#include "jansson.h"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

static Session sessions[MAX_SESSIONS]; // Array that holds all active sessions

int get_random_bytes(unsigned char *buffer, size_t length)
{
#ifdef _WIN32
    // Use CryptGenRandom on Windows to get random bytes
    HCRYPTPROV hCryptProv;
    int result = 0;

    if (CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        if (CryptGenRandom(hCryptProv, (DWORD)length, buffer))
        {
            result = 1;
        }
        CryptReleaseContext(hCryptProv, 0);
    }

    return result;
#else
    // Use /dev/urandom on Linux/macOS to get random bytes
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
    {
        return 0;
    }

    size_t bytes_read = 0;
    while (bytes_read < length)
    {
        ssize_t result = read(fd, buffer + bytes_read, length - bytes_read);
        if (result < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            close(fd);
            return 0;
        }
        bytes_read += result;
    }

    close(fd);
    return 1;
#endif
}

void generate_session_id(char *buffer)
{
    unsigned char entropy[SESSION_ID_LEN];

    // Gather entropy for random session ID generation
    if (!get_random_bytes(entropy, SESSION_ID_LEN))
    {
        // Fallback if random generation fails, using time, process ID, and a counter
        fprintf(stderr, "Random generation failed, using fallback method\n");

        unsigned int seed = (unsigned int)time(NULL);
#ifdef _WIN32
        seed ^= (unsigned int)GetCurrentProcessId();
#else
        seed ^= (unsigned int)getpid();
#endif

        static unsigned int counter = 0;
        seed ^= ++counter;

        // Use memory addresses (stack variable) to add additional entropy
        void *stack_var;
        seed ^= ((size_t)&stack_var >> 3);

        srand(seed);
        for (size_t i = 0; i < SESSION_ID_LEN; i++)
        {
            entropy[i] = (unsigned char)(rand() & 0xFF);
        }
    }

    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    for (size_t i = 0; i < SESSION_ID_LEN; i++)
    {
        buffer[i] = charset[entropy[i] % (sizeof(charset) - 1)];
    }

    memset(entropy, 0, SESSION_ID_LEN);
    buffer[SESSION_ID_LEN] = '\0';
}

void cleanup_expired_sessions()
{
    time_t now = time(NULL);
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (sessions[i].id[0] != '\0' && sessions[i].expires < now)
        {
            free_session(&sessions[i]);
        }
    }
}

char *create_session()
{
    cleanup_expired_sessions();

    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (sessions[i].id[0] == '\0')
        {
            generate_session_id(sessions[i].id);
            sessions[i].expires = time(NULL) + 3600;

            // Create empty JSON object using Jansson
            json_t *empty = json_object();
            char *empty_str = json_dumps(empty, JSON_COMPACT);

            sessions[i].data = malloc(strlen(empty_str) + 1);
            if (sessions[i].data)
            {
                strcpy(sessions[i].data, empty_str);
            }

            free(empty_str);
            json_decref(empty);

            return sessions[i].id;
        }
    }
    return NULL;
}

Session *find_session(const char *id)
{
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (strcmp(sessions[i].id, id) == 0 && sessions[i].expires >= time(NULL))
        {
            return &sessions[i];
        }
    }
    return NULL;
}

void set_session(Session *sess, const char *key, const char *value)
{
    if (!sess || !key || !value)
        return;

    // Parse existing session data
    json_error_t error;
    json_t *json = json_loads(sess->data, 0, &error);
    if (!json)
    {
        json = json_object();
    }

    // Set key-value
    json_object_set_new(json, key, json_string(value));

    // Serialize back to string
    char *updated = json_dumps(json, JSON_COMPACT);
    if (updated)
    {
        free(sess->data);
        sess->data = strdup(updated);
        free(updated);
    }

    json_decref(json);
}

void free_session(Session *sess)
{
    memset(sess->id, 0, sizeof(sess->id));
    sess->expires = 0;
    if (sess->data)
    {
        free(sess->data);
        sess->data = NULL;
    }
}

const char *get_cookie(request_t *headers, const char *name)
{
    const char *cookie_header = get_req(headers, "Cookie");
    if (!cookie_header)
        return NULL;

    static char value[256];
    const char *start = strstr(cookie_header, name);
    if (!start)
        return NULL;

    start += strlen(name);
    if (*start != '=')
        return NULL;

    start++;
    const char *end = strchr(start, ';');
    if (!end)
        end = start + strlen(start);

    size_t len = end - start;
    if (len >= sizeof(value))
        len = sizeof(value) - 1;

    strncpy(value, start, len);
    value[len] = '\0';

    return value;
}

Session *get_session(request_t *headers)
{
    const char *sid = get_cookie(headers, "session_id");
    if (!sid)
        return NULL;

    return find_session(sid);
}
