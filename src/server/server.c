#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "router.h"
#include "uv.h"

#define READ_BUF_SIZE 8192

typedef struct
{
    uv_tcp_t handle;
    uv_buf_t read_buf;
    char buffer[READ_BUF_SIZE];
    volatile int closing; // Flag to track if client is being closed
} client_t;

// Function pointers for optional pquv integration
static int (*pquv_has_active_ops_fn)(void) = NULL;
static int (*pquv_get_active_count_fn)(void) = NULL;

// Global variables for graceful shutdown
static uv_tcp_t *global_server = NULL;
static uv_signal_t sigint_handle;
#ifndef _WIN32
static uv_signal_t sigterm_handle;
#endif
#ifdef _WIN32
static uv_signal_t sigbreak_handle;
static uv_signal_t sighup_handle;
#endif
static volatile int shutdown_requested = 0;
static volatile int server_freed = 0;
static volatile int active_connections = 0;
static volatile int signal_handlers_closed = 0;

// Track signal handler initialization - FIXED: Use separate flags
static volatile int sigint_active = 0;
#ifndef _WIN32
static volatile int sigterm_active = 0;
#endif
#ifdef _WIN32
static volatile int sigbreak_active = 0;
static volatile int sighup_active = 0;
#endif

static int expected_signal_closes = 0;
static volatile int shutdown_timer_active = 0; // NEW: Track timer state

static void (*app_shutdown_hook)(void) = NULL;

// Register optional pquv functions
void register_pquv(int (*has_active_ops)(void), int (*get_active_count)(void))
{
    pquv_has_active_ops_fn = has_active_ops;
    pquv_get_active_count_fn = get_active_count;
}

// Safe wrappers for pquv functions
static inline int has_pquv_active_operations(void)
{
    return pquv_has_active_ops_fn ? pquv_has_active_ops_fn() : 0;
}

static inline int get_pquv_active_count(void)
{
    return pquv_get_active_count_fn ? pquv_get_active_count_fn() : 0;
}

void shutdown_hook(void (*hook)(void))
{
    app_shutdown_hook = hook;
}

// Allocation callback: returns the preallocated buffer for each connection.
void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    (void)suggested_size;
    client_t *client = (client_t *)handle->data;

    // More defensive checks
    if (!client || client->closing || shutdown_requested)
    {
        buf->base = NULL;
        buf->len = 0;
        return;
    }

    *buf = client->read_buf;
}

// Called when the connection is closed; frees the client struct.
void on_client_closed(uv_handle_t *handle)
{
    if (!handle)
        return;

    client_t *client = (client_t *)handle->data;
    if (client)
    {
        __sync_fetch_and_sub(&active_connections, 1);
        handle->data = NULL; // Clear the pointer first
        free(client);
    }
}

// Safe client close function - improved version
void safe_close_client(client_t *client)
{
    if (!client)
        return;

    // Use atomic operation to prevent race conditions
    if (__sync_bool_compare_and_swap(&client->closing, 0, 1))
    {
        // Only the first thread to set closing=1 will execute this block

        // Stop reading first to prevent new data from being processed
        int read_result = uv_read_stop((uv_stream_t *)&client->handle);
        if (read_result != 0 && read_result != UV_EINVAL)
        {
            fprintf(stderr, "Error stopping read: %s\n", uv_strerror(read_result));
        }

        // Then close the handle if it's not already closing
        if (!uv_is_closing((uv_handle_t *)&client->handle))
        {
            uv_close((uv_handle_t *)&client->handle, on_client_closed);
        }
        else
        {
            // If handle is already closing, we still need to decrement counter
            __sync_fetch_and_sub(&active_connections, 1);
        }
    }
}

// Called when data is read
void on_read(uv_stream_t *client_stream, ssize_t nread, const uv_buf_t *buf)
{
    if (!client_stream || !client_stream->data)
        return;

    client_t *client = (client_t *)client_stream->data;
    if (!client || client->closing)
        return;

    if (shutdown_requested)
    {
        safe_close_client(client);
        return;
    }

    if (nread < 0)
    {
        if (nread != UV_EOF && nread != UV_ECONNRESET)
            fprintf(stderr, "Read error: %s\n", uv_strerror((int)nread));

        safe_close_client(client);
        return;
    }

    if (nread == 0)
        return;

    if (buf && buf->base && nread > 0)
    {
        int should_close = router(&client->handle, buf->base, (size_t)nread);
        if (should_close)
            safe_close_client(client);
    }
    else
    {
        safe_close_client(client);
    }
}

// Called when a new connection is accepted.
void on_new_connection(uv_stream_t *server_stream, int status)
{
    if (status < 0)
    {
        fprintf(stderr, "New connection error: %s\n", uv_strerror(status));
        return;
    }

    // If shutdown has been requested, reject new connections
    if (shutdown_requested)
    {
        return;
    }

    client_t *client = malloc(sizeof(client_t));
    if (!client)
    {
        fprintf(stderr, "Failed to allocate client\n");
        return;
    }

    memset(client, 0, sizeof(client_t));
    client->closing = 0;

    // Initialize the handle
    int init_result = uv_tcp_init(uv_default_loop(), &client->handle);
    if (init_result != 0)
    {
        fprintf(stderr, "TCP init error: %s\n", uv_strerror(init_result));
        free(client);
        return;
    }

    client->handle.data = client;

    // Pre-allocate the read buffer
    client->read_buf = uv_buf_init(client->buffer, READ_BUF_SIZE);

    if (uv_accept(server_stream, (uv_stream_t *)&client->handle) == 0)
    {
        int enable = 1;
        uv_tcp_nodelay(&client->handle, enable);

        int read_result = uv_read_start((uv_stream_t *)&client->handle, alloc_buffer, on_read);
        if (read_result == 0)
        {
            __sync_fetch_and_add(&active_connections, 1);
        }
        else
        {
            fprintf(stderr, "Read start error: %s\n", uv_strerror(read_result));
            uv_close((uv_handle_t *)&client->handle, on_client_closed);
        }
    }
    else
    {
        uv_close((uv_handle_t *)&client->handle, on_client_closed);
    }
}

// Called when the server handle is closed
void on_server_closed(uv_handle_t *handle)
{
    (void)handle;
    if (global_server && !server_freed)
    {
        free(global_server);
        server_freed = 1;
        global_server = NULL;
    }
}

// FIXED: Better signal handler identification
void on_signal_closed(uv_handle_t *handle)
{
    if (!handle)
        return;

    const char *signal_name = "unknown";

    // FIXED: More reliable signal handler identification using handle addresses
    if (handle == (uv_handle_t *)&sigint_handle)
    {
        sigint_active = 0;
        signal_name = "SIGINT";
    }
#ifndef _WIN32
    else if (handle == (uv_handle_t *)&sigterm_handle)
    {
        sigterm_active = 0;
        signal_name = "SIGTERM";
    }
#endif
#ifdef _WIN32
    else if (handle == (uv_handle_t *)&sigbreak_handle)
    {
        sigbreak_active = 0;
        signal_name = "SIGBREAK";
    }
    else if (handle == (uv_handle_t *)&sighup_handle)
    {
        sighup_active = 0;
        signal_name = "SIGHUP";
    }
#endif

    int current_closed = __sync_fetch_and_add(&signal_handlers_closed, 1) + 1;
    printf("Signal handler closed: %s (%d/%d)\n", signal_name, current_closed, expected_signal_closes);
}

// FIXED: Timer close callback with proper state management
void on_timer_closed(uv_handle_t *handle)
{
    (void)handle;
    shutdown_timer_active = 0;
    printf("Shutdown timer closed\n");
}

// Used to close all client connections
void walk_callback(uv_handle_t *handle, void *arg)
{
    (void)arg;
    if (uv_is_closing(handle))
        return;

    if (handle->type == UV_TCP && global_server && handle != (uv_handle_t *)global_server)
    {
        // This is a client connection
        client_t *client = (client_t *)handle->data;
        if (client)
        {
            safe_close_client(client);
        }
    }
    else if (handle->type == UV_TIMER)
    {
        // Don't close shutdown timer here - it's managed separately
        if (shutdown_timer_active && handle->data != (void *)0xDEADBEEF)
        {
            fprintf(stderr, "Closing remaining timer...\n");
            uv_close(handle, on_timer_closed);
        }
    }
    else if (handle->type == UV_ASYNC)
    {
        fprintf(stderr, "Closing remaining async handle...\n");
        uv_close(handle, NULL);
    }
    else if (handle->type == UV_POLL)
    {
        fprintf(stderr, "Closing UV_POLL handle...\n");
        uv_close(handle, NULL);
    }
}

// DEBUG
static void count_handles(uv_handle_t *h, void *arg)
{
    int *cnt = arg;
    if (!uv_is_closing(h))
    {
        ++*cnt;
        fprintf(stderr, "OPEN HANDLE: %s\n", uv_handle_type_name(h->type));
    }
}

// DEBUG
void report_open_handles(uv_loop_t *loop)
{
    int count = 0;
    uv_walk(loop, count_handles, &count);
    if (count > 0)
        fprintf(stderr, ">>> %d handle(s) still open\n", count);
}

void close_remaining_handles(uv_handle_t *handle, void *arg)
{
    (void)arg;

    if (uv_is_closing(handle))
        return;

    printf("Force closing handle type: %s\n", uv_handle_type_name(handle->type));

    switch (handle->type)
    {
    case UV_TIMER:
        uv_timer_stop((uv_timer_t *)handle);
        break;
    case UV_POLL:
        uv_poll_stop((uv_poll_t *)handle);
        break;
    case UV_TCP:
        // Don't close server handle here as it's handled elsewhere
        if (handle != (uv_handle_t *)global_server)
        {
            client_t *client = (client_t *)handle->data;
            if (client && !client->closing)
            {
                client->closing = 1;
                uv_read_stop((uv_stream_t *)handle);
            }
        }
        break;
    case UV_SIGNAL:
        break; // Handled in graceful shutdown
    case UV_ASYNC:
        break;
    default:
        break;
    }

    uv_close(handle, NULL);
}

// FIXED: Proper timer management
static uv_timer_t shutdown_timer;

// Timeout callback for graceful shutdown
void shutdown_timeout_cb(uv_timer_t *handle)
{
    printf("Shutdown timeout reached, forcing closure of remaining handles...\n");
    uv_walk(uv_default_loop(), close_remaining_handles, NULL);
}

// FIXED: Safe signal handler close function with better state tracking
static void safe_close_signal_handler(uv_signal_t *handle, volatile int *active_flag, const char *name)
{
    if (*active_flag && !uv_is_closing((uv_handle_t *)handle))
    {
        printf("Closing %s signal handler...\n", name);
        uv_signal_stop(handle);
        uv_close((uv_handle_t *)handle, on_signal_closed);
        expected_signal_closes++;
    }
}

// FIXED: Graceful shutdown procedure with better error handling
void graceful_shutdown()
{
    if (shutdown_requested)
        return;

    shutdown_requested = 1;
    printf("Initiating graceful shutdown...\n");

    // Close server first to prevent new connections
    if (global_server && !uv_is_closing((uv_handle_t *)global_server))
    {
        uv_close((uv_handle_t *)global_server, on_server_closed);
    }

    // Call app shutdown hook
    if (app_shutdown_hook)
    {
        app_shutdown_hook();
    }

    // FIXED: Reset counters properly
    expected_signal_closes = 0;
    signal_handlers_closed = 0;

    // FIXED: Close signal handlers with better identification
    safe_close_signal_handler(&sigint_handle, &sigint_active, "SIGINT");
#ifndef _WIN32
    safe_close_signal_handler(&sigterm_handle, &sigterm_active, "SIGTERM");
#endif
#ifdef _WIN32
    safe_close_signal_handler(&sigbreak_handle, &sigbreak_active, "SIGBREAK");
    safe_close_signal_handler(&sighup_handle, &sighup_active, "SIGHUP");
#endif

    printf("Expected signal closes: %d\n", expected_signal_closes);

    // Close client connections
    uv_walk(uv_default_loop(), walk_callback, NULL);

    // FIXED: Setup shutdown timeout timer with proper management
    if (!shutdown_timer_active)
    {
        memset(&shutdown_timer, 0, sizeof(shutdown_timer));
        if (uv_timer_init(uv_default_loop(), &shutdown_timer) == 0)
        {
            shutdown_timer.data = (void *)0xDEADBEEF; // Mark as shutdown timer
            shutdown_timer_active = 1;
            uv_timer_start(&shutdown_timer, shutdown_timeout_cb, 15000, 0);
        }
    }

    // Wait for connections and database to close
    int wait_cycles = 0;
    while (uv_loop_alive(uv_default_loop()) && wait_cycles < 300) // 30 seconds max
    {
        // Check if shutdown conditions are met
        if (active_connections == 0 &&
            !has_pquv_active_operations() &&
            signal_handlers_closed >= expected_signal_closes)
        {
            printf("All operations completed, finishing shutdown...\n");
            break;
        }

        if (wait_cycles % 50 == 0 && wait_cycles > 0) // Every 5 seconds
        {
            int pquv_count = get_pquv_active_count();
            printf("Waiting for shutdown: %d connections, %d async ops, %d/%d signals closed\n",
                   active_connections, pquv_count, signal_handlers_closed, expected_signal_closes);
            report_open_handles(uv_default_loop());
        }

        uv_run(uv_default_loop(), UV_RUN_ONCE);
        wait_cycles++;
    }

    // FIXED: Properly stop and close shutdown timer
    if (shutdown_timer_active && !uv_is_closing((uv_handle_t *)&shutdown_timer))
    {
        uv_timer_stop(&shutdown_timer);
        uv_close((uv_handle_t *)&shutdown_timer, on_timer_closed);

        // Wait for timer to close
        int timer_wait = 0;
        while (shutdown_timer_active && uv_loop_alive(uv_default_loop()) && timer_wait < 50)
        {
            uv_run(uv_default_loop(), UV_RUN_ONCE);
            timer_wait++;
        }
    }

    printf("Attempting to close event loop...\n");
    report_open_handles(uv_default_loop());

    // Try to close the loop
    int rc = uv_loop_close(uv_default_loop());
    if (rc != 0)
    {
        printf("Failed to close loop: %s\n", uv_strerror(rc));
        report_open_handles(uv_default_loop());

        // FIXED: More aggressive cleanup
        printf("Force closing remaining handles...\n");

        // Multiple passes with handle type specific cleanup
        for (int pass = 0; pass < 5; pass++)
        {
            printf("Cleanup pass %d\n", pass + 1);
            uv_walk(uv_default_loop(), close_remaining_handles, NULL);

            // Run loop longer to process closes
            int iterations = 0;
            while (uv_loop_alive(uv_default_loop()) && iterations < 50)
            {
                uv_run(uv_default_loop(), UV_RUN_NOWAIT);
                iterations++;
            }

            // Check if loop is now closeable
            rc = uv_loop_close(uv_default_loop());
            if (rc == 0)
            {
                printf("Loop closed successfully after pass %d\n", pass + 1);
                break;
            }
        }

        if (rc != 0)
        {
            printf("CRITICAL: Could not close event loop: %s\n", uv_strerror(rc));
            report_open_handles(uv_default_loop());
            printf("WARNING: Event loop could not be cleanly closed\n");
        }
    }
    else
    {
        printf("Loop closed successfully\n");
    }

    printf("Server shutdown completed\n");
}

// FIXED: Signal callback with reentrancy protection
static volatile int signal_handler_running = 0;

void signal_handler(uv_signal_t *handle, int signum)
{
    // Prevent reentrancy
    if (__sync_bool_compare_and_swap(&signal_handler_running, 0, 1))
    {
        switch (signum)
        {
        case SIGINT:
            printf("Received SIGINT, shutting down...\n");
            break;
#ifndef _WIN32
        case SIGTERM:
            printf("Received SIGTERM, shutting down...\n");
            break;
#endif
#ifdef _WIN32
        case SIGBREAK:
            printf("Received SIGBREAK, shutting down...\n");
            break;
        case SIGHUP:
            printf("Received SIGHUP, shutting down...\n");
            break;
#endif
        default:
            printf("Received signal %d, shutting down...\n", signum);
            break;
        }

        graceful_shutdown();
        signal_handler_running = 0;
    }
}

// FIXED: Better state reset function
static void reset_shutdown_state()
{
    shutdown_requested = 0;
    server_freed = 0;
    active_connections = 0;
    signal_handlers_closed = 0;
    expected_signal_closes = 0;
    shutdown_timer_active = 0;
    signal_handler_running = 0;
    global_server = NULL;

    // Reset signal handler state flags
    sigint_active = 0;
#ifndef _WIN32
    sigterm_active = 0;
#endif
#ifdef _WIN32
    sigbreak_active = 0;
    sighup_active = 0;
#endif
}

// FIXED: Server startup function with better error handling
void ecewo(unsigned short PORT)
{
    uv_loop_t *loop = uv_default_loop();
    uv_tcp_t *server = malloc(sizeof(*server));
    if (!server)
    {
        fprintf(stderr, "Failed to allocate server\n");
        exit(1);
    }

    // Initialize variables
    reset_shutdown_state();

    int init_result = uv_tcp_init(loop, server);
    if (init_result != 0)
    {
        fprintf(stderr, "TCP init error: %s\n", uv_strerror(init_result));
        free(server);
        exit(1);
    }

    // Bind the server socket to the specified port
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", PORT, &addr);
    int r = uv_tcp_bind(server, (const struct sockaddr *)&addr, 0);
    if (r)
    {
        fprintf(stderr, "Bind error: %s\n", uv_strerror(r));
        free(server);
        exit(1);
    }

    // Start listening for incoming connections
    uv_tcp_simultaneous_accepts(server, 1);
    r = uv_listen((uv_stream_t *)server, 128, on_new_connection);
    if (r)
    {
        fprintf(stderr, "Listen error: %s\n", uv_strerror(r));
        free(server);
        exit(1);
    }

    // A valid server handle is now available
    global_server = server;

    // FIXED: Initialize and start signal handlers with proper error checking
    memset(&sigint_handle, 0, sizeof(sigint_handle));
    if (uv_signal_init(loop, &sigint_handle) == 0)
    {
        if (uv_signal_start(&sigint_handle, signal_handler, SIGINT) == 0)
        {
            sigint_active = 1;
            printf("SIGINT handler initialized\n");
        }
        else
        {
            fprintf(stderr, "Failed to start SIGINT handler\n");
        }
    }

#ifndef _WIN32
    memset(&sigterm_handle, 0, sizeof(sigterm_handle));
    if (uv_signal_init(loop, &sigterm_handle) == 0)
    {
        if (uv_signal_start(&sigterm_handle, signal_handler, SIGTERM) == 0)
        {
            sigterm_active = 1;
            printf("SIGTERM handler initialized\n");
        }
        else
        {
            fprintf(stderr, "Failed to start SIGTERM handler\n");
        }
    }
#endif

#ifdef _WIN32
    memset(&sigbreak_handle, 0, sizeof(sigbreak_handle));
    if (uv_signal_init(loop, &sigbreak_handle) == 0)
    {
        if (uv_signal_start(&sigbreak_handle, signal_handler, SIGBREAK) == 0)
        {
            sigbreak_active = 1;
            printf("SIGBREAK handler initialized\n");
        }
        else
        {
            fprintf(stderr, "Failed to start SIGBREAK handler\n");
        }
    }

    memset(&sighup_handle, 0, sizeof(sighup_handle));
    if (uv_signal_init(loop, &sighup_handle) == 0)
    {
        if (uv_signal_start(&sighup_handle, signal_handler, SIGHUP) == 0)
        {
            sighup_active = 1;
            printf("SIGHUP handler initialized\n");
        }
        else
        {
            fprintf(stderr, "Failed to start SIGHUP handler\n");
        }
    }
#endif

    printf("Server is running at: http://localhost:%d\n", PORT);

    // Main event loop: runs until a signal stops it
    uv_run(loop, UV_RUN_DEFAULT);

    printf("Main loop exited, beginning cleanup...\n");

    // Wait for all handles to be closed properly
    int max_iterations = 200;
    int iterations = 0;
    while (uv_loop_alive(loop) && iterations < max_iterations)
    {
        uv_run(loop, UV_RUN_NOWAIT);
        iterations++;
        if (iterations % 20 == 0)
        {
            printf("Waiting for handles to close... (iteration %d)\n", iterations);
            report_open_handles(loop);
        }
    }

    if (iterations >= max_iterations)
    {
        printf("Timeout reached, forcing final cleanup...\n");
        uv_walk(loop, close_remaining_handles, NULL);

        // Give more time for forced cleanup
        for (int i = 0; i < 50; i++)
        {
            if (!uv_loop_alive(loop))
                break;
            uv_run(loop, UV_RUN_NOWAIT);
        }
    }

    // Try to close the loop
    int close_result = uv_loop_close(loop);
    if (close_result != 0)
    {
        fprintf(stderr, "Failed to close loop: %s\n", uv_strerror(close_result));
        report_open_handles(loop);

        // Last resort: try to force close everything
        printf("Last resort cleanup...\n");
        uv_walk(loop, close_remaining_handles, NULL);

        // Process remaining events
        for (int i = 0; i < 50; i++)
        {
            if (!uv_loop_alive(loop))
                break;
            uv_run(loop, UV_RUN_NOWAIT);
        }

        close_result = uv_loop_close(loop);
        if (close_result != 0)
        {
            fprintf(stderr, "CRITICAL: Could not close event loop: %s\n", uv_strerror(close_result));
            fprintf(stderr, "This may indicate a serious resource leak\n");
        }
        else
        {
            printf("Loop finally closed after aggressive cleanup\n");
        }
    }
    else
    {
        printf("Event loop closed successfully\n");
    }

    // Clean up server memory if not already done
    if (!server_freed && global_server)
    {
        free(global_server);
        server_freed = 1;
    }

    reset_shutdown_state();

    printf("Server shutdown completed successfully\n");
}
