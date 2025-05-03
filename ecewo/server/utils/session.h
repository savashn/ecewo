#ifndef SESSION_H
#define SESSION_H

#include <time.h>
#include "router.h"

#define SESSION_ID_LEN 32 // Length of the session ID (32 characters)
#define MAX_SESSIONS 100  // Maximum number of active sessions

// Session structure to hold session information
typedef struct
{
    char id[SESSION_ID_LEN + 1]; // Unique session ID (32 bytes + null terminator)
    char *data;                  // Data associated with the session (e.g., "Welcome message")
    time_t expires;              // Expiration time of the session (UNIX timestamp)
} Session;

// Function to generate a random session ID
// Fills the provided buffer with a randomly generated session ID
void generate_session_id(char *buffer);

// Function to create a new session
// Creates a new session, generates a session ID, and initializes session data
// Returns the session ID if creation is successful, NULL otherwise
char *create_session();

// Function to find a session by its ID
// Searches for a session using the provided session ID
// Returns a pointer to the session if found and not expired, NULL otherwise
Session *find_session(const char *id);

// Function to set a key-value pair in the session's data (stored as JSON)
// Updates the session data (JSON format) with the given key and value
void set_session(Session *sess, const char *key, const char *value);

// Function to free a session and its associated resources
// Clears session ID, expiration time, and frees session data
void free_session(Session *sess);

// Function to get the value of a specific cookie from request headers
// Searches for the cookie with the given name and returns its value
// Returns NULL if the cookie is not found
const char *get_cookie(request_t *headers, const char *name);

// Function to get the authenticated session from request cookies
// Extracts the session ID from the cookies in the request headers
// Returns the session if found and authenticated, NULL otherwise
Session *get_session(request_t *headers);

#endif
