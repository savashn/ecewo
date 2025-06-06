#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "session.h"
#include "request.h"
#include "cJSON.h"
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
            return NULL; // Failed to initialize
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

    // If no empty slot found, try to resize
    if (empty_slot == -1)
    {
        if (!resize_sessions(max_sessions * 2))
        {
            return NULL; // Failed to resize
        }
        empty_slot = max_sessions / 2; // Use the first slot in the new section
    }

    // Create new session
    generate_session_id(sessions[empty_slot].id);
    sessions[empty_slot].expires = time(NULL) + max_age;

    cJSON *empty = cJSON_CreateObject();
    char *empty_str = cJSON_PrintUnformatted(empty);

    sessions[empty_slot].data = malloc(strlen(empty_str) + 1);
    if (sessions[empty_slot].data)
    {
        memcpy(sessions[empty_slot].data, empty_str, strlen(empty_str) + 1);
    }

    cJSON_Delete(empty);
    free(empty_str);

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

    // Parse existing session data
    cJSON *json = cJSON_Parse(sess->data);
    if (!json)
    {
        json = cJSON_CreateObject(); // Create a new JSON object if parsing fails
    }

    // Set key-value
    cJSON_AddStringToObject(json, key, value);

    // Serialize back to string
    char *updated = cJSON_PrintUnformatted(json);

    if (updated)
    {
        free(sess->data);
        sess->data = strdup(updated);
        free(updated);
    }

    cJSON_Delete(json);
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
