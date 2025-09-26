#ifndef ECEWO_SERVER_H
#define ECEWO_SERVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uv.h"
#include "../lib/middleware.h"

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
    SERVER_INVALID_PORT = -8,        // Invalid PORT
} server_error_t;

// ============================================================================
// CALLBACK TYPE DEFINITIONS
// ============================================================================

// Called when server begins graceful shutdown process
// Use this to cleanup application resources, close databases, etc.
typedef void (*shutdown_callback_t)(void);

// Used for set_timeout and set_interval operations
typedef void (*timer_callback_t)(void *user_data);

// ============================================================================
// CORE SERVER API
// ============================================================================

// Must be called before any other server operations
// returns SERVER_OK on success, error code on failure
int server_init(void);

// Server must be initialized before calling this
// returns SERVER_OK on success, error code on failure
int server_listen(uint16_t port);

// Run the server event loop
// This function blocks until the server is shut down
// Automatically calls server_cleanup() when loop exits
void server_run(void);

// ============================================================================
// CONFIGURATION API
// ============================================================================

// Set shutdown callback function
// Function to call during shutdown (can be NULL)
void shutdown_hook(shutdown_callback_t callback);

// ============================================================================
// STATUS API
// ============================================================================

// Check if server is currently running
bool server_is_running(void);

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
// ROUTER
// ============================================================================

// GET
#define GET_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define get(...) \
    GET_CHOOSER(__VA_ARGS__, get_with_mw, get_no_mw)(__VA_ARGS__)

static inline void get_no_mw(const char *path, RequestHandler handler)
{
    register_route("GET", path, NO_MW, handler);
}

static inline void get_with_mw(
    const char *path,
    MiddlewareArray mw,
    RequestHandler handler)
{
    register_route("GET", path, mw, handler);
}

// POST
#define POST_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define post(...) \
    POST_CHOOSER(__VA_ARGS__, post_with_mw, post_no_mw)(__VA_ARGS__)

static inline void post_no_mw(const char *p, RequestHandler h)
{
    register_route("POST", p, NO_MW, h);
}

static inline void post_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_route("POST", p, mw, h);
}

// PUT
#define PUT_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define put(...) \
    PUT_CHOOSER(__VA_ARGS__, put_with_mw, put_no_mw)(__VA_ARGS__)

static inline void put_no_mw(const char *p, RequestHandler h)
{
    register_route("PUT", p, NO_MW, h);
}

static inline void put_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_route("PUT", p, mw, h);
}

// PATCH
#define PATCH_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define patch(...) \
    PATCH_CHOOSER(__VA_ARGS__, patch_with_mw, patch_no_mw)(__VA_ARGS__)

static inline void patch_no_mw(const char *p, RequestHandler h)
{
    register_route("PATCH", p, NO_MW, h);
}

static inline void patch_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_route("PATCH", p, mw, h);
}

// DELETE
#define DEL_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define del(...) \
    DEL_CHOOSER(__VA_ARGS__, del_with_mw, del_no_mw)(__VA_ARGS__)

static inline void del_no_mw(const char *p, RequestHandler h)
{
    register_route("DELETE", p, NO_MW, h);
}

static inline void del_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_route("DELETE", p, mw, h);
}

#endif
