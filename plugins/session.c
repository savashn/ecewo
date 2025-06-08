#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "session.h"
#include "request.h"
#include "compat.h"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

// Dynamic session storage
static Session *sessions = NULL; // Pointer to array of sessions
static int max_sessions = 0;     // Current capacity of the sessions array
static int initialized = 0;      // Flag to check if sessions are initialized

// Initialize the session system
int init_sessions(void)
{
    if (initialized)
    {
        return 1; // Already initialized
    }

    const int initial_capacity = MAX_SESSIONS_DEFAULT;

    sessions = (Session *)malloc(initial_capacity * sizeof(Session));
    if (!sessions)
    {
        return 0; // Memory allocation failed
    }

    // Initialize all session slots to empty
    for (int i = 0; i < initial_capacity; i++)
    {
        sessions[i].id[0] = '\0';
        sessions[i].data = NULL;
        sessions[i].expires = 0;
    }

    max_sessions = initial_capacity;
    initialized = 1;

    return 1;
}

// Clean up and free all session resources
void final_sessions(void)
{
    if (!initialized)
    {
        return;
    }

    // Free all session data
    for (int i = 0; i < max_sessions; i++)
    {
        if (sessions[i].id[0] != '\0' && sessions[i].data != NULL)
        {
            free(sessions[i].data);
            sessions[i].data = NULL;
        }
    }

    // Free the sessions array
    free(sessions);
    sessions = NULL;
    max_sessions = 0;
    initialized = 0;
}

// Resize the sessions array if needed
static int resize_sessions(int new_capacity)
{
    if (new_capacity <= max_sessions)
    {
        return 1; // No need to resize
    }

    Session *new_sessions = (Session *)realloc(sessions, new_capacity * sizeof(Session));
    if (!new_sessions)
    {
        return 0; // Memory allocation failed
    }

    // Initialize new session slots
    for (int i = max_sessions; i < new_capacity; i++)
    {
        new_sessions[i].id[0] = '\0';
        new_sessions[i].data = NULL;
        new_sessions[i].expires = 0;
    }

    sessions = new_sessions;
    max_sessions = new_capacity;

    return 1;
}

static int get_random_bytes(unsigned char *buffer, size_t length)
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

static void generate_session_id(char *buffer)
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

static void cleanup_expired_sessions()
{
    if (!initialized)
    {
        if (!init_sessions())
        {
            return; // Failed to initialize
        }
    }

    time_t now = time(NULL);
    for (int i = 0; i < max_sessions; i++)
    {
        if (sessions[i].id[0] != '\0' && sessions[i].expires < now)
        {
            free_session(&sessions[i]);
        }
    }
}

char *create_session(int max_age)
{
    if (!initialized)
    {
        if (!init_sessions())
        {
            return NULL; // Initialization failed
        }
    }

    cleanup_expired_sessions();

    // Find an empty slot
    int empty_slot = -1;
    for (int i = 0; i < max_sessions; i++)
    {
        if (sessions[i].id[0] == '\0')
        {
            empty_slot = i;
            break;
        }
    }

    // If no empty slot, resize
    if (empty_slot == -1)
    {
        if (!resize_sessions(max_sessions * 2))
        {
            return NULL; // Resize failed
        }
        empty_slot = max_sessions / 2;
    }

    // Generate session ID
    generate_session_id(sessions[empty_slot].id);
    sessions[empty_slot].expires = time(NULL) + max_age;

    // Initialize with an empty key-value string
    sessions[empty_slot].data = malloc(1);
    if (!sessions[empty_slot].data)
    {
        return NULL; // Memory error
    }

    sessions[empty_slot].data[0] = '\0'; // Empty string: ""

    return sessions[empty_slot].id;
}

Session *find_session(const char *id)
{
    if (!initialized || !id)
    {
        return NULL;
    }

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

void set_session(Session *sess, const char *key, const char *value)
{
    if (!sess || !key || !value)
        return;

    size_t key_len = strlen(key);
    size_t value_len = strlen(value);

    // Create the new key-value pair, e.g. "username":"janedoe"
    size_t pair_len = key_len + value_len + 6; // "", "", :
    char *new_pair = malloc(pair_len);
    if (!new_pair)
        return;

    sprintf(new_pair, "\"%s\":\"%s\"", key, value);

    // If data is empty, create new JSON object
    if (!sess->data || strlen(sess->data) == 0)
    {
        size_t total_len = pair_len + 3;
        sess->data = malloc(total_len);
        if (!sess->data)
        {
            free(new_pair);
            return;
        }
        sprintf(sess->data, "{%s}", new_pair);
        free(new_pair);
        return;
    }

    // Check if the key already exists in data
    char *pos = strstr(sess->data, key);
    if (pos && *(pos - 1) == '"' && *(pos + key_len) == '"')
    {
        // If in form "key":"...value...", remove old value
        char *start = strchr(pos, ':');
        if (!start)
        {
            free(new_pair);
            return;
        }
        start++; // After the ":"

        if (*start != '"')
        {
            free(new_pair);
            return;
        }

        start++; // Start of value

        char *end = strchr(start, '"');
        if (!end)
        {
            free(new_pair);
            return;
        }

        end++; // Closing quote
        if (*end == ',')
            end++;

        // Replace old pair with new one
        size_t old_len = end - (pos - 1); // including quote
        size_t new_data_len = strlen(sess->data) - old_len + pair_len + 3;

        char *new_data = malloc(new_data_len);
        if (!new_data)
        {
            free(new_pair);
            return;
        }

        size_t head_len = (pos - 1) - sess->data;
        strncpy(new_data, sess->data, head_len);
        new_data[head_len] = '\0';

        strcat(new_data, new_pair);

        if (*end != '}')
            strcat(new_data, ",");

        strcat(new_data, end);

        free(sess->data);
        sess->data = new_data;
    }
    else
    {
        // Key not found, append before the final } with comma
        size_t existing_len = strlen(sess->data);
        size_t new_len = existing_len + pair_len + 2;

        char *new_data = malloc(new_len);
        if (!new_data)
        {
            free(new_pair);
            return;
        }

        strcpy(new_data, sess->data);
        if (existing_len > 1 && new_data[existing_len - 1] == '}')
        {
            new_data[existing_len - 1] = '\0';
            strcat(new_data, ",");
            strcat(new_data, new_pair);
            strcat(new_data, "}");
        }

        free(sess->data);
        sess->data = new_data;
    }

    free(new_pair);
}

void free_session(Session *sess)
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

Session *get_session(request_t *headers)
{
    // Extract the session_id cookie (heap-allocated)
    char *sid = handle_get_cookie(headers, "session_id");
    if (!sid)
        return NULL;

    Session *sess = find_session(sid);
    free(sid);
    return sess;
}
