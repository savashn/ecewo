#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "session.h"
#include "router.h"
#include "request.h"
#include "lib/cjson.h"

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
        seed ^= ((size_t)&stack_var >> 3); // Lower bits may not be highly variable

        srand(seed);
        for (size_t i = 0; i < SESSION_ID_LEN; i++)
        {
            entropy[i] = (unsigned char)(rand() & 0xFF);
        }
    }

    // URL-safe Base64 character set for encoding the session ID
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    // Map each byte to a character in the charset
    for (size_t i = 0; i < SESSION_ID_LEN; i++)
    {
        buffer[i] = charset[entropy[i] % (sizeof(charset) - 1)];
    }

    // Clear memory of entropy to avoid leaking information about the algorithm
    memset(entropy, 0, SESSION_ID_LEN);

    // Null-terminate the string
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
    // Clean up the expired sessions
    cleanup_expired_sessions();

    // Look for an empty slot in the sessions array
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (sessions[i].id[0] == '\0')
        {
            generate_session_id(sessions[i].id);     // Generate a new session ID
            sessions[i].expires = time(NULL) + 3600; // Set session expiration time (1 hour)

            // Create an empty JSON object for session data
            cJSON *empty = cJSON_CreateObject();
            char *empty_str = cJSON_PrintUnformatted(empty);

            // Dynamically allocate memory for session data
            sessions[i].data = malloc(strlen(empty_str) + 1); // Allocate memory for the string
            if (sessions[i].data)
            {
                strcpy(sessions[i].data, empty_str); // Copy the empty JSON string to session data
            }

            cJSON_Delete(empty);
            free(empty_str);

            return sessions[i].id; // Return the new session ID
        }
    }
    return NULL; // Return NULL if no empty session slot is found
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

// Function to set a key-value pair in the session's data (stored as JSON)
void set_session(Session *sess, const char *key, const char *value)
{
    if (!sess || !key || !value)
        return; // Validate inputs

    // Parse existing session data as JSON
    cJSON *json = cJSON_Parse(sess->data);
    if (!json)
    {
        json = cJSON_CreateObject(); // Create a new JSON object if parsing fails
    }

    // Add the key-value pair to the JSON object
    cJSON_AddStringToObject(json, key, value);

    // Convert the JSON object back to a string
    char *updated = cJSON_PrintUnformatted(json);

    // Free old session data and update with the new string
    if (updated)
    {
        free(sess->data);             // Free old session data
        sess->data = strdup(updated); // Allocate memory and copy the updated string
        free(updated);                // Free the temporary string
    }

    cJSON_Delete(json); // Free the JSON object
}

// Function to free a session and clear its data
void free_session(Session *sess)
{
    memset(sess->id, 0, sizeof(sess->id)); // Clear the session ID
    sess->expires = 0;                     // Reset the expiration time
    if (sess->data)
    {
        free(sess->data);  // Free session data
        sess->data = NULL; // Nullify the data pointer
    }
}

// Function to retrieve a cookie's value by name from the request headers
const char *get_cookie(request_t *headers, const char *name)
{
    const char *cookie_header = get_req(headers, "Cookie");
    if (!cookie_header)
        return NULL; // Return NULL if no cookie header is found

    static char value[256]; // Buffer to hold the cookie value

    // Example: Cookie: session_id=xyz123; other=abc
    const char *start = strstr(cookie_header, name);
    if (!start)
        return NULL; // Return NULL if cookie name is not found

    start += strlen(name);
    if (*start != '=')
        return NULL; // Return NULL if '=' is not found after the cookie name

    start++; // Skip '=' character
    const char *end = strchr(start, ';');
    if (!end)
        end = start + strlen(start); // If no ';' found, go to the end of the string

    size_t len = end - start;
    if (len >= sizeof(value))
        len = sizeof(value) - 1;

    strncpy(value, start, len); // Copy the cookie value into the buffer
    value[len] = '\0';          // Null-terminate the string

    return value; // Return the cookie value
}

// Function to get the authenticated session by reading the session ID from cookies
Session *get_session(request_t *headers)
{
    const char *sid = get_cookie(headers, "session_id");
    if (!sid)
        return NULL; // Return NULL if no session ID cookie is found

    return find_session(sid); // Look up and return the session if found
}
