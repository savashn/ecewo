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

// Track signal handler initialization
static volatile int sigint_active = 0;
#ifndef _WIN32
static volatile int sigterm_active = 0;
#endif
#ifdef _WIN32
static volatile int sigbreak_active = 0;
static volatile int sighup_active = 0;
#endif

static int expected_signal_closes = 0;
static volatile int shutdown_timer_active = 0;
static uv_timer_t shutdown_timer;

// Add shutdown state tracking
typedef enum
{
    SHUTDOWN_STATE_RUNNING,
    SHUTDOWN_STATE_INITIATED,
    SHUTDOWN_STATE_CLOSING_SERVER,
    SHUTDOWN_STATE_CLOSING_CONNECTIONS,
    SHUTDOWN_STATE_CLOSING_SIGNALS,
    SHUTDOWN_STATE_WAITING_ASYNC,
    SHUTDOWN_STATE_FORCE_CLEANUP,
    SHUTDOWN_STATE_COMPLETE
} shutdown_state_t;

static volatile shutdown_state_t shutdown_state = SHUTDOWN_STATE_RUNNING;
static uint64_t shutdown_start_time = 0;

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

// Better timeout calculation
static uint64_t get_shutdown_elapsed_ms(void)
{
    if (shutdown_start_time == 0)
        return 0;
    return (uv_hrtime() - shutdown_start_time) / 1000000;
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
        int remaining = __sync_fetch_and_sub(&active_connections, 1) - 1;
        printf("Client connection closed, %d remaining\n", remaining);
        handle->data = NULL; // Clear the pointer first
        free(client);
    }
}

// Safe client close function with better error handling
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
            int remaining = __sync_fetch_and_sub(&active_connections, 1) - 1;
            printf("Handle already closing, %d connections remaining\n", remaining);
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
        printf("Rejecting new connection due to shutdown\n");
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
            int current = __sync_fetch_and_add(&active_connections, 1) + 1;
            printf("New client connected, %d total connections\n", current);
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
    printf("Server handle closed\n");
    shutdown_state = SHUTDOWN_STATE_CLOSING_CONNECTIONS;

    if (global_server && !server_freed)
    {
        free(global_server);
        server_freed = 1;
        global_server = NULL;
    }
}

// Better signal handler identification and state tracking
void on_signal_closed(uv_handle_t *handle)
{
    if (!handle)
        return;

    const char *signal_name = "unknown";

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

    if (current_closed >= expected_signal_closes)
    {
        shutdown_state = SHUTDOWN_STATE_WAITING_ASYNC;
    }
}

// Timer close callback
void on_timer_closed(uv_handle_t *handle)
{
    (void)handle;
    shutdown_timer_active = 0;
    printf("Shutdown timer closed\n");
}

// More comprehensive handle walking with state awareness
void walk_callback(uv_handle_t *handle, void *arg)
{
    (void)arg;
    if (uv_is_closing(handle))
        return;

    if (handle->type == UV_TCP && global_server && handle != (uv_handle_t *)global_server)
    {
        // This is a client connection
        client_t *client = (client_t *)handle->data;
        if (client && !client->closing)
        {
            printf("Closing client connection during shutdown\n");
            safe_close_client(client);
        }
    }
    else if (handle->type == UV_TIMER)
    {
        // Don't close shutdown timer here - it's managed separately
        if (shutdown_timer_active && handle != (uv_handle_t *)&shutdown_timer)
        {
            printf("Closing remaining timer...\n");
            uv_timer_stop((uv_timer_t *)handle);
            uv_close(handle, NULL);
        }
    }
    else if (handle->type == UV_ASYNC)
    {
        printf("Closing async handle...\n");
        uv_close(handle, NULL);
    }
    else if (handle->type == UV_POLL)
    {
        printf("Closing poll handle...\n");
        uv_poll_stop((uv_poll_t *)handle);
        uv_close(handle, NULL);
    }
}

// DEBUG: Enhanced handle counting with more details
static void count_handles(uv_handle_t *h, void *arg)
{
    int *cnt = arg;
    if (!uv_is_closing(h))
    {
        ++*cnt;
        const char *type_name = uv_handle_type_name(h->type);
        printf("OPEN HANDLE: %s (active: %d, closing: %d)\n",
               type_name, uv_is_active(h), uv_is_closing(h));
    }
}

void report_open_handles(uv_loop_t *loop)
{
    int count = 0;
    uv_walk(loop, count_handles, &count);
    if (count > 0)
        printf(">>> %d handle(s) still open\n", count);
    else
        printf(">>> All handles closed\n");
}

// More aggressive cleanup with proper prioritization
void close_remaining_handles(uv_handle_t *handle, void *arg)
{
    (void)arg;

    if (uv_is_closing(handle))
        return;

    printf("Force closing handle type: %s\n", uv_handle_type_name(handle->type));

    // Stop handles first, then close
    switch (handle->type)
    {
    case UV_TIMER:
        uv_timer_stop((uv_timer_t *)handle);
        break;
    case UV_POLL:
        uv_poll_stop((uv_poll_t *)handle);
        break;
    case UV_TCP:
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
        uv_signal_stop((uv_signal_t *)handle);
        break;
    case UV_ASYNC:
        break; // No stop method needed
    default:
        break;
    }

    uv_close(handle, NULL);
}

// Timeout callback with better state management
void shutdown_timeout_cb(uv_timer_t *handle)
{
    uint64_t elapsed = get_shutdown_elapsed_ms();
    printf("Shutdown timeout reached after %llu ms, forcing closure...\n",
           (unsigned long long)elapsed);

    shutdown_state = SHUTDOWN_STATE_FORCE_CLEANUP;

    // Force close all remaining handles
    uv_walk(uv_default_loop(), close_remaining_handles, NULL);
}

// Safe signal handler close with better coordination
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

// Enhanced graceful shutdown with better state management
void graceful_shutdown()
{
    if (shutdown_requested)
        return;

    shutdown_requested = 1;
    shutdown_start_time = uv_hrtime();
    shutdown_state = SHUTDOWN_STATE_INITIATED;

    printf("=== GRACEFUL SHUTDOWN INITIATED ===\n");

    // Phase 1: Close server to prevent new connections
    shutdown_state = SHUTDOWN_STATE_CLOSING_SERVER;
    if (global_server && !uv_is_closing((uv_handle_t *)global_server))
    {
        printf("Phase 1: Closing server to stop new connections...\n");
        uv_close((uv_handle_t *)global_server, on_server_closed);
    }
    else
    {
        shutdown_state = SHUTDOWN_STATE_CLOSING_CONNECTIONS;
    }

    // Phase 2: Setup signal handler cleanup
    shutdown_state = SHUTDOWN_STATE_CLOSING_SIGNALS;
    expected_signal_closes = 0;
    signal_handlers_closed = 0;

    safe_close_signal_handler(&sigint_handle, &sigint_active, "SIGINT");
#ifndef _WIN32
    safe_close_signal_handler(&sigterm_handle, &sigterm_active, "SIGTERM");
#endif
#ifdef _WIN32
    safe_close_signal_handler(&sigbreak_handle, &sigbreak_active, "SIGBREAK");
    safe_close_signal_handler(&sighup_handle, &sighup_active, "SIGHUP");
#endif

    printf("Expected signal closes: %d\n", expected_signal_closes);

    // Phase 3: Close client connections
    printf("Phase 3: Closing client connections...\n");
    uv_walk(uv_default_loop(), walk_callback, NULL);

    // Phase 4: Setup shutdown timeout timer
    if (!shutdown_timer_active)
    {
        memset(&shutdown_timer, 0, sizeof(shutdown_timer));
        if (uv_timer_init(uv_default_loop(), &shutdown_timer) == 0)
        {
            shutdown_timer.data = (void *)0xDEADBEEF;
            shutdown_timer_active = 1;
            uv_timer_start(&shutdown_timer, shutdown_timeout_cb, 30000, 0);
            printf("Shutdown timer started (30 seconds)\n");
        }
    }

    // Phase 5: Wait for operations to complete
    shutdown_state = SHUTDOWN_STATE_WAITING_ASYNC;
    printf("Phase 5: Waiting for operations to complete...\n");

    int wait_cycles = 0;
    const int max_wait_cycles = 600;

    while (uv_loop_alive(uv_default_loop()) && wait_cycles < max_wait_cycles &&
           shutdown_state != SHUTDOWN_STATE_FORCE_CLEANUP)
    {
        int connections = active_connections;
        int pquv_active = has_pquv_active_operations();
        int signals_closed = signal_handlers_closed;

        if (connections == 0 && !pquv_active && signals_closed >= expected_signal_closes)
        {
            printf("All operations completed naturally, finishing shutdown...\n");
            shutdown_state = SHUTDOWN_STATE_COMPLETE;
            break;
        }

        if (wait_cycles % 50 == 0 && wait_cycles > 0)
        {
            uint64_t elapsed = get_shutdown_elapsed_ms();
            printf("Shutdown progress (%llu ms): %d connections, %d async ops, %d/%d signals\n",
                   (unsigned long long)elapsed, connections,
                   get_pquv_active_count(), signals_closed, expected_signal_closes);
        }

        uv_run(uv_default_loop(), UV_RUN_ONCE);
        wait_cycles++;
    }

    if (app_shutdown_hook)
    {
        printf("Calling application shutdown hook...\n");
        app_shutdown_hook();
    }

    // Phase 6: Final cleanup
    printf("Phase 6: Final cleanup...\n");

    // Stop and close shutdown timer
    if (shutdown_timer_active && !uv_is_closing((uv_handle_t *)&shutdown_timer))
    {
        uv_timer_stop(&shutdown_timer);
        uv_close((uv_handle_t *)&shutdown_timer, on_timer_closed);

        int timer_wait = 0;
        while (shutdown_timer_active && uv_loop_alive(uv_default_loop()) && timer_wait < 50)
        {
            uv_run(uv_default_loop(), UV_RUN_ONCE);
            timer_wait++;
        }
    }

    printf("Attempting to close event loop...\n");
    report_open_handles(uv_default_loop());

    int rc = uv_loop_close(uv_default_loop());
    if (rc != 0)
    {
        printf("Failed to close loop: %s\n", uv_strerror(rc));
        printf("Performing aggressive cleanup...\n");

        for (int pass = 0; pass < 3; pass++)
        {
            printf("Aggressive cleanup pass %d\n", pass + 1);
            uv_walk(uv_default_loop(), close_remaining_handles, NULL);

            for (int i = 0; i < 100; i++)
            {
                if (!uv_loop_alive(uv_default_loop()))
                    break;
                uv_run(uv_default_loop(), UV_RUN_NOWAIT);
            }

            rc = uv_loop_close(uv_default_loop());
            if (rc == 0)
            {
                printf("Loop closed successfully after aggressive cleanup pass %d\n", pass + 1);
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
        printf("Event loop closed successfully\n");
    }

    shutdown_state = SHUTDOWN_STATE_COMPLETE;
    uint64_t total_time = get_shutdown_elapsed_ms();
    printf("=== GRACEFUL SHUTDOWN COMPLETED (%llu ms) ===\n",
           (unsigned long long)total_time);
}

// Signal callback with reentrancy protection
static volatile int signal_handler_running = 0;

void signal_handler(uv_signal_t *handle, int signum)
{
    // Prevent reentrancy
    if (__sync_bool_compare_and_swap(&signal_handler_running, 0, 1))
    {
        switch (signum)
        {
        case SIGINT:
            printf("\n=== RECEIVED SIGINT ===\n");
            break;
#ifndef _WIN32
        case SIGTERM:
            printf("\n=== RECEIVED SIGTERM ===\n");
            break;
#endif
#ifdef _WIN32
        case SIGBREAK:
            printf("\n=== RECEIVED SIGBREAK ===\n");
            break;
        case SIGHUP:
            printf("\n=== RECEIVED SIGHUP ===\n");
            break;
#endif
        default:
            printf("\n=== RECEIVED SIGNAL %d ===\n", signum);
            break;
        }

        graceful_shutdown();
        signal_handler_running = 0;
    }
}

// Better state reset function
static void reset_shutdown_state()
{
    shutdown_requested = 0;
    server_freed = 0;
    active_connections = 0;
    signal_handlers_closed = 0;
    expected_signal_closes = 0;
    shutdown_timer_active = 0;
    signal_handler_running = 0;
    shutdown_start_time = 0;
    shutdown_state = SHUTDOWN_STATE_RUNNING;
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

// Server startup function with enhanced error handling
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

    // Server is now ready
    global_server = server;

    // Initialize signal handlers with proper error checking
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
    }

    memset(&sighup_handle, 0, sizeof(sighup_handle));
    if (uv_signal_init(loop, &sighup_handle) == 0)
    {
        if (uv_signal_start(&sighup_handle, signal_handler, SIGHUP) == 0)
        {
            sighup_active = 1;
            printf("SIGHUP handler initialized\n");
        }
    }
#endif

    printf("=== SERVER STARTED ===\n");
    printf("Server running at: http://localhost:%d\n", PORT);
    printf("Press Ctrl+C to initiate graceful shutdown\n");

    // Main event loop
    uv_run(loop, UV_RUN_DEFAULT);

    printf("Main loop exited, performing final cleanup...\n");

    // Final cleanup phase
    int max_iterations = 200;
    int iterations = 0;
    while (uv_loop_alive(loop) && iterations < max_iterations)
    {
        uv_run(loop, UV_RUN_NOWAIT);
        iterations++;
        if (iterations % 20 == 0)
        {
            printf("Final cleanup iteration %d/%d\n", iterations, max_iterations);
        }
    }

    if (iterations >= max_iterations)
    {
        printf("Final cleanup timeout, forcing handle closure...\n");
        uv_walk(loop, close_remaining_handles, NULL);

        for (int i = 0; i < 50; i++)
        {
            if (!uv_loop_alive(loop))
                break;
            uv_run(loop, UV_RUN_NOWAIT);
        }
    }

    // Final loop close attempt
    int close_result = uv_loop_close(loop);
    if (close_result != 0)
    {
        fprintf(stderr, "Failed to close loop in final cleanup: %s\n", uv_strerror(close_result));
        report_open_handles(loop);
    }
    else
    {
        printf("Event loop closed successfully in final cleanup\n");
    }

    // Clean up server memory if not already done
    if (!server_freed && global_server)
    {
        free(global_server);
        server_freed = 1;
    }

    reset_shutdown_state();
    printf("=== SERVER SHUTDOWN COMPLETE ===\n");
}
