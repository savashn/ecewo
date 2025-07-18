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
    volatile int closing; // Flag to track if client is being closed
} client_t;

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

static void (*app_shutdown_hook)(void) = NULL;

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

// Called when a signal handler is closed
void on_signal_closed(uv_handle_t *handle)
{
    __sync_fetch_and_add(&signal_handlers_closed, 1);
}

// Timer close callback
void on_timer_closed(uv_handle_t *handle)
{
    (void)handle;
    printf("Timer closed\n");
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
        fprintf(stderr, "Closing remaining timer...\n");
        uv_close(handle, on_timer_closed);
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

// Graceful shutdown procedure
void graceful_shutdown()
{
    if (shutdown_requested)
        return;

    shutdown_requested = 1;

    if (app_shutdown_hook)
        app_shutdown_hook();

    // First, close all client connections
    uv_walk(uv_default_loop(), walk_callback, NULL);

    // Wait for connections to close with better timeout handling
    int wait_iterations = 0;
    int max_wait = 200; // Increase timeout

    while (active_connections > 0 && wait_iterations < max_wait)
    {
        uv_run(uv_default_loop(), UV_RUN_NOWAIT);
        wait_iterations++;

        if (wait_iterations % 40 == 0) // Less frequent logging
        {
            printf("Waiting for %d connections to close... (iter %d)\n",
                   active_connections, wait_iterations);
        }

        if (active_connections > 0)
        {
            sleep_ms(50); // Longer delay
        }
    }

    if (active_connections > 0)
    {
        printf("Warning: %d connections still active after timeout\n", active_connections);
    }

    // Then close the server
    if (global_server && !uv_is_closing((uv_handle_t *)global_server))
    {
        uv_close((uv_handle_t *)global_server, on_server_closed);
    }

    // Stop and close signal handlers
    uv_signal_stop(&sigint_handle);
#ifndef _WIN32
    uv_signal_stop(&sigterm_handle);
#endif
#ifdef _WIN32
    uv_signal_stop(&sigbreak_handle);
    uv_signal_stop(&sighup_handle);
#endif

    // Close signal handlers
    if (!uv_is_closing((uv_handle_t *)&sigint_handle))
        uv_close((uv_handle_t *)&sigint_handle, on_signal_closed);
#ifndef _WIN32
    if (!uv_is_closing((uv_handle_t *)&sigterm_handle))
        uv_close((uv_handle_t *)&sigterm_handle, on_signal_closed);
#endif
#ifdef _WIN32
    if (!uv_is_closing((uv_handle_t *)&sigbreak_handle))
        uv_close((uv_handle_t *)&sigbreak_handle, on_signal_closed);
    if (!uv_is_closing((uv_handle_t *)&sighup_handle))
        uv_close((uv_handle_t *)&sighup_handle, on_signal_closed);
#endif
}

// Signal callback: triggered when signals are received
void signal_handler(uv_signal_t *handle, int signum)
{
    (void)handle;
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
}

// Server startup function
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
    server_freed = 0;
    signal_handlers_closed = 0;
    active_connections = 0;
    shutdown_requested = 0;

    uv_tcp_init(loop, server);

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

    // Initialize and start signal handlers
    uv_signal_init(loop, &sigint_handle);
#ifndef _WIN32
    uv_signal_init(loop, &sigterm_handle);
#endif
#ifdef _WIN32
    uv_signal_init(loop, &sigbreak_handle);
    uv_signal_init(loop, &sighup_handle);
#endif

    uv_signal_start(&sigint_handle, signal_handler, SIGINT);
#ifndef _WIN32
    uv_signal_start(&sigterm_handle, signal_handler, SIGTERM);
#endif
#ifdef _WIN32
    uv_signal_start(&sigbreak_handle, signal_handler, SIGBREAK);
    uv_signal_start(&sighup_handle, signal_handler, SIGHUP);
#endif

    printf("Server is running at: http://localhost:%d\n", PORT);

    // Main event loop: runs until a signal stops it
    uv_run(loop, UV_RUN_DEFAULT);

    // Wait for all handles to be closed properly
    int max_iterations = 150;
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

    // Try to close the loop
    int close_result = uv_loop_close(loop);
    if (close_result != 0)
    {
        fprintf(stderr, "Failed to close loop: %s\n", uv_strerror(close_result));

        // Force close remaining handles
        uv_walk(loop, walk_callback, NULL);

        // Give more time for cleanup
        iterations = 0;
        while (uv_loop_alive(loop) && iterations < 50)
        {
            uv_run(loop, UV_RUN_NOWAIT);
            iterations++;
        }

        close_result = uv_loop_close(loop);
        if (close_result != 0)
        {
            fprintf(stderr, "Still failed to close loop: %s\n", uv_strerror(close_result));
        }
    }

    // Clean up server memory if not already done
    if (!server_freed && global_server)
    {
        free(global_server);
        server_freed = 1;
    }

    global_server = NULL;
    printf("Server shutdown completed successfully\n");
}
