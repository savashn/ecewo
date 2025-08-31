#ifndef ECEWO_SERVER_H
#define ECEWO_SERVER_H

#include <stdint.h>
#include <stddef.h>
#include "uv.h"

// ============================================================================
// ERROR CODES
// ============================================================================

typedef enum
{
    SERVER_OK = 0,                   // Success
    SERVER_ALREADY_INITIALIZED = -1, // Server already initialized
    SERVER_NOT_INITIALIZED = -2,     // Server not initialized
    SERVER_ALREADY_RUNNING = -3,     // Server already running
    SERVER_INIT_FAILED = -4,         // Initialization failed
    SERVER_OUT_OF_MEMORY = -5,       // Memory allocation failed
    SERVER_BIND_FAILED = -6,         // Socket bind failed
    SERVER_LISTEN_FAILED = -7,       // Socket listen failed
} server_error_t;

// ============================================================================
// CALLBACK TYPE DEFINITIONS
// ============================================================================

// Called when non-fatal errors occur during server operation
typedef void (*error_callback_t)(const char *error_message);

// Called when server begins graceful shutdown process
// Use this to cleanup application resources, close databases, etc.
typedef void (*shutdown_callback_t)(void);

// Used for set_timeout and set_interval operations
typedef void (*timer_callback_t)(void *user_data);

// ============================================================================
// CORE SERVER API
// ============================================================================

// Must be called before any other server operations
// returns ECEWO_OK on success, error code on failure
int server_init(void);

// Server must be initialized before calling this
// returns ECEWO_OK on success, error code on failure
int server_listen(int port);

// Run the server event loop
// This function blocks until the server is shut down
// Automatically calls server_cleanup() when loop exits
void server_run(void);

// Clean up server resources
// Called automatically by server_run() or can be called manually
void server_cleanup(void);

// ============================================================================
// CONFIGURATION API
// ============================================================================

// Set error callback function
// Function to call when errors occur (can be NULL)
void error_hook(error_callback_t callback);

// Set shutdown callback function
// Function to call during shutdown (can be NULL)
void shutdown_hook(shutdown_callback_t callback);

// ============================================================================
// STATUS API
// ============================================================================

// Check if server is currently running
// returns 1 if running, 0 if not
int server_is_running(void);

// Get number of active client connections
int get_active_connections(void);

// Get the libuv event loop handle
// Useful for integrating with other libuv-based libraries
// returns pointer to uv_loop_t or NULL if not initialized
uv_loop_t *get_loop(void);

// ============================================================================
// ASYNC UTILITIES API
// ============================================================================

// Execute callback after specified delay (similar to JavaScript setTimeout)
// returns timer handle on success, NULL on failure
uv_timer_t *set_timeout(timer_callback_t callback, uint64_t delay_ms, void *user_data);

// Execute callback repeatedly at specified intervals (similar to JavaScript setInterval)
// returns timer handle on success, NULL on failure
uv_timer_t *set_interval(timer_callback_t callback, uint64_t interval_ms, void *user_data);

// Cancel a timer created with set_timeout or set_interval
void clear_timer(uv_timer_t *timer);

// ============================================================================
// FROM ECEWO.C
// ============================================================================

void router_init(void);
void router_cleanup(void);

#endif
