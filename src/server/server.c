#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif
#include "router.h"
#include "uv.h"

#define READ_BUF_SIZE 8192

typedef struct
{
    uv_tcp_t handle;
    uv_buf_t read_buf;
    char buffer[READ_BUF_SIZE];
    volatile int closing;
} client_t;

// Function pointers for PQUV integration
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
static volatile int signal_handlers_initialized = 0;

static void (*app_shutdown_hook)(void) = NULL;

// Register PQUV functions for graceful shutdown coordination
void register_pquv(int (*has_active_ops)(void), int (*get_active_count)(void))
{
    pquv_has_active_ops_fn = has_active_ops;
    pquv_get_active_count_fn = get_active_count;
    printf("PQUV functions registered with server\n");
}

// Safe wrappers for PQUV functions
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

// Safe client close function
void safe_close_client(client_t *client)
{
    if (!client)
        return;

    if (__sync_bool_compare_and_swap(&client->closing, 0, 1))
    {
        uv_read_stop((uv_stream_t *)&client->handle);

        if (!uv_is_closing((uv_handle_t *)&client->handle))
        {
            uv_close((uv_handle_t *)&client->handle, on_client_closed);
        }
        else
        {
            __sync_fetch_and_sub(&active_connections, 1);
        }
    }
}

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

void on_new_connection(uv_stream_t *server_stream, int status)
{
    if (status < 0)
    {
        fprintf(stderr, "New connection error: %s\n", uv_strerror(status));
        return;
    }

    if (shutdown_requested)
        return;

    client_t *client = malloc(sizeof(client_t));
    if (!client)
        return;

    memset(client, 0, sizeof(client_t));
    client->closing = 0;

    if (uv_tcp_init(uv_default_loop(), &client->handle) != 0)
    {
        free(client);
        return;
    }

    client->handle.data = client;
    client->read_buf = uv_buf_init(client->buffer, READ_BUF_SIZE);

    if (uv_accept(server_stream, (uv_stream_t *)&client->handle) == 0)
    {
        uv_tcp_nodelay(&client->handle, 1);

        if (uv_read_start((uv_stream_t *)&client->handle, alloc_buffer, on_read) == 0)
        {
            __sync_fetch_and_add(&active_connections, 1);
        }
        else
        {
            uv_close((uv_handle_t *)&client->handle, on_client_closed);
        }
    }
    else
    {
        uv_close((uv_handle_t *)&client->handle, on_client_closed);
    }
}

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

void walk_callback(uv_handle_t *handle, void *arg)
{
    (void)arg;
    if (uv_is_closing(handle))
        return;

    if (handle->type == UV_TCP && global_server && handle != (uv_handle_t *)global_server)
    {
        client_t *client = (client_t *)handle->data;
        if (client)
        {
            safe_close_client(client);
        }
    }
    else if (handle->type == UV_TIMER || handle->type == UV_ASYNC || handle->type == UV_POLL)
    {
        uv_close(handle, NULL);
    }
}

void force_close_callback(uv_handle_t *handle, void *arg)
{
    (void)arg;
    if (uv_is_closing(handle))
        return;

    const char *handle_type_names[] = {
        "UNKNOWN_HANDLE", "ASYNC", "CHECK", "FS_EVENT", "FS_POLL",
        "HANDLE", "IDLE", "NAMED_PIPE", "POLL", "PREPARE", "PROCESS",
        "STREAM", "TCP", "TIMER", "TTY", "UDP", "SIGNAL", "FILE"};

    const char *type_name = (handle->type < 18) ? handle_type_names[handle->type] : "UNKNOWN";

    printf("Force closing %s handle\n", type_name);
    uv_close(handle, NULL);
}

void on_signal_closed(uv_handle_t *handle)
{
    // Verify this is actually a signal handle
    if (!handle || handle->type != UV_SIGNAL)
    {
        printf("WARNING: on_signal_closed called for non-signal handle (type=%d)\n",
               handle ? handle->type : -1);
        return;
    }

    // Prevent double-counting with atomic operation
    static volatile int close_call_count = 0;
    int current_count = __sync_fetch_and_add(&close_call_count, 1) + 1;

    // Don't count beyond initialized handlers
    if (current_count > signal_handlers_initialized)
    {
        printf("WARNING: Signal handler closed callback called too many times (%d/%d)\n",
               current_count, signal_handlers_initialized);
        return;
    }

    int closed_count = __sync_fetch_and_add(&signal_handlers_closed, 1) + 1;
    printf("Signal handler closed (%d/%d)\n", closed_count, signal_handlers_initialized);

    // Stop the main loop when all signal handlers are closed
    if (closed_count == signal_handlers_initialized && shutdown_requested)
    {
        printf("All signal handlers closed, stopping main loop\n");
        uv_stop(uv_default_loop());
    }
}

void graceful_shutdown()
{
    if (__sync_bool_compare_and_swap(&shutdown_requested, 0, 1))
    {
        printf("=== GRACEFUL SHUTDOWN INITIATED ===\n");

        // 1. Close server to prevent new connections
        if (global_server && !uv_is_closing((uv_handle_t *)global_server))
        {
            printf("Closing server to stop new connections...\n");
            uv_close((uv_handle_t *)global_server, on_server_closed);
        }

        // 2. Close all client connections FIRST
        printf("Closing client connections...\n");
        uv_walk(uv_default_loop(), walk_callback, NULL);

        // 3. Wait for client connections to close before database cleanup
        printf("Waiting for client connections to close...\n");
        int wait_iterations = 0;
        int max_wait = 100; // 5 seconds for clients

        while (wait_iterations < max_wait && active_connections > 0)
        {
            uv_run(uv_default_loop(), UV_RUN_NOWAIT);
            wait_iterations++;
            if (wait_iterations % 20 == 0)
            {
                printf("Waiting for %d client connections...\n", active_connections);
            }
            sleep_ms(50);
        }

        // 4. NOW call app shutdown hook (database cleanup)
        if (app_shutdown_hook)
        {
            printf("Calling application shutdown hook...\n");
            app_shutdown_hook();
        }

        // 5. Wait for database operations to complete
        printf("Waiting for database operations to complete...\n");
        wait_iterations = 0;
        max_wait = 200; // 10 seconds max

        while (wait_iterations < max_wait)
        {
            int pquv_active = has_pquv_active_operations();
            int pquv_count = get_pquv_active_count();

            if (!pquv_active && active_connections == 0)
            {
                printf("All operations completed, finishing shutdown...\n");
                break;
            }

            if (wait_iterations % 20 == 0 && wait_iterations > 0)
            {
                if (pquv_get_active_count_fn)
                {
                    printf("Waiting: %d connections, %d database operations\n",
                           active_connections, pquv_count);
                }
                else
                {
                    printf("Waiting: %d connections\n", active_connections);
                }
            }

            uv_run(uv_default_loop(), UV_RUN_NOWAIT);
            wait_iterations++;
            sleep_ms(50);
        }

        // 6. Stop and close signal handlers PROPERLY
        printf("Closing signal handlers...\n");
        signal_handlers_closed = 0; // Reset counter

        // Stop all signal handlers first - this prevents new signals from being handled
        if (uv_is_active((uv_handle_t *)&sigint_handle))
        {
            uv_signal_stop(&sigint_handle);
        }
#ifndef _WIN32
        if (uv_is_active((uv_handle_t *)&sigterm_handle))
        {
            uv_signal_stop(&sigterm_handle);
        }
#endif
#ifdef _WIN32
        if (uv_is_active((uv_handle_t *)&sigbreak_handle))
        {
            uv_signal_stop(&sigbreak_handle);
        }
        if (uv_is_active((uv_handle_t *)&sighup_handle))
        {
            uv_signal_stop(&sighup_handle);
        }
#endif

        // Then close them - only if not already closing
        if (!uv_is_closing((uv_handle_t *)&sigint_handle))
        {
            uv_close((uv_handle_t *)&sigint_handle, on_signal_closed);
        }
        else
        {
            signal_handlers_closed++;
        }

#ifndef _WIN32
        if (!uv_is_closing((uv_handle_t *)&sigterm_handle))
        {
            uv_close((uv_handle_t *)&sigterm_handle, on_signal_closed);
        }
        else
        {
            signal_handlers_closed++;
        }
#endif
#ifdef _WIN32
        if (!uv_is_closing((uv_handle_t *)&sigbreak_handle))
        {
            uv_close((uv_handle_t *)&sigbreak_handle, on_signal_closed);
        }
        else
        {
            signal_handlers_closed++;
        }

        if (!uv_is_closing((uv_handle_t *)&sighup_handle))
        {
            uv_close((uv_handle_t *)&sighup_handle, on_signal_closed);
        }
        else
        {
            signal_handlers_closed++;
        }
#endif

        // 7. Wait for signal handlers to close
        printf("Waiting for signal handlers to close...\n");
        wait_iterations = 0;
        max_wait = 100; // 5 seconds

        while (signal_handlers_closed < signal_handlers_initialized && wait_iterations < max_wait)
        {
            uv_run(uv_default_loop(), UV_RUN_NOWAIT);
            wait_iterations++;
            if (wait_iterations % 20 == 0)
            {
                printf("Waiting for signal handlers: %d/%d closed\n",
                       signal_handlers_closed, signal_handlers_initialized);
            }
            sleep_ms(50);
        }

        // 8. Final cleanup - don't run forever
        printf("Final handle cleanup...\n");

        // Run event loop until no more active handles, but with a limit
        int cleanup_iterations = 0;
        while (uv_loop_alive(uv_default_loop()) && cleanup_iterations < 100)
        {
            uv_run(uv_default_loop(), UV_RUN_NOWAIT);
            cleanup_iterations++;
            sleep_ms(10);
        }

        if (cleanup_iterations >= 100)
        {
            printf("WARNING: Cleanup timeout reached after %d iterations\n", cleanup_iterations);
        }

        printf("=== GRACEFUL SHUTDOWN COMPLETED ===\n");

        sigint_handle.signal_cb = NULL;
#ifndef _WIN32
        sigterm_handle.signal_cb = NULL;
#endif
#ifdef _WIN32
        sigbreak_handle.signal_cb = NULL;
        sighup_handle.signal_cb = NULL;
#endif
    }
    else
    {
        printf("Shutdown already in progress...\n");
    }
}

void signal_handler(uv_signal_t *handle, int signum)
{
    (void)handle;

    // Prevent multiple signal handling during shutdown
    if (shutdown_requested)
    {
        printf("Signal %d received but shutdown already in progress\n", signum);
        return;
    }

    printf("\nReceived signal %d, shutting down gracefully...\n", signum);
    graceful_shutdown();
}

static void debug_walk_callback(uv_handle_t *handle, void *arg)
{
    (void)arg;

    const char *handle_type_names[] = {
        "UNKNOWN_HANDLE", "ASYNC", "CHECK", "FS_EVENT", "FS_POLL",
        "HANDLE", "IDLE", "NAMED_PIPE", "POLL", "PREPARE", "PROCESS",
        "STREAM", "TCP", "TIMER", "TTY", "UDP", "SIGNAL", "FILE"};

    const char *type_name = (handle->type < 18) ? handle_type_names[handle->type] : "UNKNOWN";

    printf("Remaining handle: type=%d (%s), closing=%s, active=%s, data=%p",
           handle->type, type_name,
           uv_is_closing(handle) ? "yes" : "no",
           uv_is_active(handle) ? "yes" : "no",
           handle->data);

    // Special info for signal handles
    if (handle->type == UV_SIGNAL)
    {
        uv_signal_t *sig = (uv_signal_t *)handle;
        printf(", signum=%d", sig->signum);
    }

    printf("\n");
}

void ecewo(unsigned short PORT)
{
    printf("Main loop starting with UV_RUN_DEFAULT\n");

    uv_loop_t *loop = uv_default_loop();
    uv_tcp_t *server = malloc(sizeof(*server));
    if (!server)
    {
        fprintf(stderr, "Failed to allocate server\n");
        exit(1);
    }

    // Initialize variables
    server_freed = 0;
    active_connections = 0;
    shutdown_requested = 0;
    signal_handlers_closed = 0;
    signal_handlers_initialized = 0;

    uv_tcp_init(loop, server);

    // Bind and listen
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", PORT, &addr);
    int r = uv_tcp_bind(server, (const struct sockaddr *)&addr, 0);
    if (r)
    {
        fprintf(stderr, "Bind error: %s\n", uv_strerror(r));
        free(server);
        exit(1);
    }

    uv_tcp_simultaneous_accepts(server, 1);
    r = uv_listen((uv_stream_t *)server, 128, on_new_connection);
    if (r)
    {
        fprintf(stderr, "Listen error: %s\n", uv_strerror(r));
        free(server);
        exit(1);
    }

    global_server = server;

    // Initialize signal handlers
    uv_signal_init(loop, &sigint_handle);
    uv_signal_start(&sigint_handle, signal_handler, SIGINT);
    signal_handlers_initialized++;

#ifndef _WIN32
    uv_signal_init(loop, &sigterm_handle);
    uv_signal_start(&sigterm_handle, signal_handler, SIGTERM);
    signal_handlers_initialized++;
#endif
#ifdef _WIN32
    uv_signal_init(loop, &sigbreak_handle);
    uv_signal_init(loop, &sighup_handle);
    uv_signal_start(&sigbreak_handle, signal_handler, SIGBREAK);
    uv_signal_start(&sighup_handle, signal_handler, SIGHUP);
    signal_handlers_initialized += 2;
#endif

    printf("=== SERVER STARTED ===\n");
    printf("Server running at: http://localhost:%d\n", PORT);
    printf("Press Ctrl+C to shutdown\n");
    printf("Signal handlers initialized: %d\n", signal_handlers_initialized);

    // Main event loop
    uv_run(loop, UV_RUN_DEFAULT);

    printf("UV_RUN_DEFAULT returned, main loop exited\n");

    // Check if any handles are still active
    if (uv_loop_alive(loop))
    {
        printf("Loop still has active handles, checking...\n");
        uv_walk(loop, debug_walk_callback, NULL);

        // Force close remaining handles
        uv_walk(loop, force_close_callback, NULL);

        // Run a few more times to process closes
        for (int i = 0; i < 10; i++)
        {
            uv_run(loop, UV_RUN_NOWAIT);
            sleep_ms(10);
        }
    }

    // Try to close the loop
    int close_result = uv_loop_close(loop);
    if (close_result != 0)
    {
        printf("Failed to close loop: %s\n", uv_strerror(close_result));
        printf("Checking for remaining handles...\n");
        uv_walk(loop, debug_walk_callback, NULL);
    }
    else
    {
        printf("Loop closed successfully\n");
    }

    // Final server cleanup
    if (!server_freed && global_server)
    {
        free(global_server);
        server_freed = 1;
    }

    global_server = NULL;
    printf("=== SERVER SHUTDOWN COMPLETE ===\n");
}
