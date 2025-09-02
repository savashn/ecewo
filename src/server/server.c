#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "server.h"
#include "../lib/route_trie.h"

#define READ_BUFFER_SIZE 16384
#define MAX_CONNECTIONS 10000
#define LISTEN_BACKLOG 511
#define IDLE_TIMEOUT_SECONDS 120  // 2 minutes idle timeout
#define CLEANUP_INTERVAL_MS 60000 // Check every minute

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

typedef struct client_s
{
    uv_tcp_t handle;
    uv_buf_t read_buf;
    char buffer[READ_BUFFER_SIZE];
    int closing;
    time_t last_activity;   // Only field needed for idle tracking
    int keep_alive_enabled; // Track if this connection uses keep-alive
    struct client_s *next;  // Linked list for easy iteration
} client_t;

typedef struct timer_data_s
{
    timer_callback_t callback;
    void *user_data;
    int is_interval;
} timer_data_t;

// Global server state
static struct
{
    int initialized;
    int running;
    int shutdown_requested;
    int active_connections;

    uv_loop_t *loop;
    uv_tcp_t *server;
    uv_signal_t sigint_handle;
    uv_signal_t sigterm_handle;

    shutdown_callback_t shutdown_callback;
    error_callback_t error_callback;

    client_t *client_list_head; // Head of client linked list
    uv_timer_t *cleanup_timer;  // Single timer for all cleanup

    int server_closed;
} g_server = {0};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static void on_connection(uv_stream_t *server, int status);
static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
static void on_signal(uv_signal_t *handle, int signum);
static void close_client(client_t *client);
static void cleanup_idle_connections(uv_timer_t *handle);
static void close_walk_cb(uv_handle_t *handle, void *arg);
static void on_server_closed(uv_handle_t *handle);
static int router_init(void);
static void router_cleanup(void);

// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

// Add client to linked list
static void add_client_to_list(client_t *client)
{
    client->next = g_server.client_list_head;
    g_server.client_list_head = client;
}

// Remove client from linked list
static void remove_client_from_list(client_t *client)
{
    if (!client)
        return;

    if (g_server.client_list_head == client)
    {
        g_server.client_list_head = client->next;
        return;
    }

    client_t *current = g_server.client_list_head;
    while (current && current->next != client)
    {
        current = current->next;
    }

    if (current)
    {
        current->next = client->next;
    }
}

// Periodic cleanup of idle connections
static void cleanup_idle_connections(uv_timer_t *handle)
{
    (void)handle;

    if (g_server.shutdown_requested)
    {
        return;
    }

    time_t now = time(NULL);
    client_t *current = g_server.client_list_head;

    while (current)
    {
        client_t *next = current->next; // Always save next first

        if (current->keep_alive_enabled && !current->closing)
        {
            time_t idle_time = now - current->last_activity;
            if (idle_time > IDLE_TIMEOUT_SECONDS)
            {
                close_client(current); // This removes from list
            }
        }
        current = next;
    }
}

// Start the cleanup timer
static int start_cleanup_timer(void)
{
    g_server.cleanup_timer = malloc(sizeof(uv_timer_t));
    if (!g_server.cleanup_timer)
    {
        return -1;
    }

    if (uv_timer_init(g_server.loop, g_server.cleanup_timer) != 0)
    {
        free(g_server.cleanup_timer);
        g_server.cleanup_timer = NULL;
        return -1;
    }

    // Start periodic cleanup (every 30 seconds)
    if (uv_timer_start(g_server.cleanup_timer, cleanup_idle_connections,
                       CLEANUP_INTERVAL_MS, CLEANUP_INTERVAL_MS) != 0)
    {
        uv_close((uv_handle_t *)g_server.cleanup_timer, (uv_close_cb)free);
        g_server.cleanup_timer = NULL;
        return -1;
    }

    return 0;
}

// Stop the cleanup timer
static void stop_cleanup_timer(void)
{
    if (g_server.cleanup_timer)
    {
        uv_timer_stop(g_server.cleanup_timer);
        uv_close((uv_handle_t *)g_server.cleanup_timer, (uv_close_cb)free);
        g_server.cleanup_timer = NULL;
    }
}

// ============================================================================
// CORE SERVER API
// ============================================================================

int server_init(void)
{
    if (g_server.initialized)
    {
        return SERVER_ALREADY_INITIALIZED;
    }

    memset(&g_server, 0, sizeof(g_server));

    g_server.loop = uv_default_loop();
    if (!g_server.loop)
    {
        return SERVER_INIT_FAILED;
    }

    // Setup signal handlers
    if (uv_signal_init(g_server.loop, &g_server.sigint_handle) != 0 ||
        uv_signal_init(g_server.loop, &g_server.sigterm_handle) != 0)
    {
        return SERVER_INIT_FAILED;
    }

    uv_signal_start(&g_server.sigint_handle, on_signal, SIGINT);
    uv_signal_start(&g_server.sigterm_handle, on_signal, SIGTERM);

    // Initialize router system
    if (router_init() != 0)
    {
        return SERVER_INIT_FAILED;
    }

    g_server.initialized = 1;

    atexit(server_cleanup);

    return SERVER_OK;
}

int server_listen(int port)
{
    if (port < 1 || port > 65535)
    {
        fprintf(stderr, "Error: Invalid port %d (must be 1-65535)\n", port);
        return SERVER_INVALID_PORT;
    }

    if (!g_server.initialized)
    {
        return SERVER_NOT_INITIALIZED;
    }

    if (g_server.running)
    {
        return SERVER_ALREADY_RUNNING;
    }

    // Allocate server handle
    g_server.server = malloc(sizeof(uv_tcp_t));
    if (!g_server.server)
    {
        return SERVER_OUT_OF_MEMORY;
    }

    // Initialize TCP server
    if (uv_tcp_init(g_server.loop, g_server.server) != 0)
    {
        free(g_server.server);
        g_server.server = NULL;
        return SERVER_INIT_FAILED;
    }

    // Production TCP settings
    uv_tcp_simultaneous_accepts(g_server.server, 1);

    // Bind to port
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", port, &addr);

    if (uv_tcp_bind(g_server.server, (const struct sockaddr *)&addr, 0) != 0)
    {
        free(g_server.server);
        g_server.server = NULL;
        fprintf(stderr, "Error: Failed to bind to port %d (may be in use)\n", port);
        return SERVER_BIND_FAILED;
    }

    // Start listening
    if (uv_listen((uv_stream_t *)g_server.server, LISTEN_BACKLOG, on_connection) != 0)
    {
        free(g_server.server);
        g_server.server = NULL;
        fprintf(stderr, "Error: Failed to listen on port %d\n", port);
        return SERVER_LISTEN_FAILED;
    }

    // Start cleanup timer for idle connections
    if (start_cleanup_timer() != 0)
    {
        printf("Warning: Failed to start cleanup timer\n");
    }

    g_server.running = 1;
    printf("Server listening on http://localhost:%d\n", port);

    return SERVER_OK;
}

void server_run(void)
{
    if (!g_server.initialized || !g_server.running)
    {
        if (g_server.error_callback)
        {
            g_server.error_callback("Server not initialized or not listening");
        }
        return;
    }

    printf("Press Ctrl+C to stop the server\n");

    // Heart of the web server - simple event loop
    uv_run(g_server.loop, UV_RUN_DEFAULT);

    // Automatic cleanup when loop ends
    server_cleanup();
}

void server_shutdown(void)
{
    if (g_server.shutdown_requested)
    {
        return;
    }

    g_server.shutdown_requested = 1;
    g_server.running = 0;

    // Call user cleanup
    if (g_server.shutdown_callback)
    {
        g_server.shutdown_callback();
    }

    // Stop cleanup timer first
    stop_cleanup_timer();

    // Stop signal handlers
    uv_signal_stop(&g_server.sigint_handle);
    uv_signal_stop(&g_server.sigterm_handle);

    // Close all connections and timers
    uv_walk(g_server.loop, close_walk_cb, NULL);

    // Wait for connections to close
    int wait_count = 0;
    while (g_server.active_connections > 0)
    {
        uv_run(g_server.loop, UV_RUN_ONCE);
        wait_count++;

        // Progress report every 10 iterations
        if (wait_count % 10 == 0)
        {
            printf("Waiting for %d connections to close...\n",
                   g_server.active_connections);
        }
    }

    // Close server handle
    if (g_server.server && !uv_is_closing((uv_handle_t *)g_server.server))
    {
        uv_close((uv_handle_t *)g_server.server, on_server_closed);
    }
}

void server_cleanup(void)
{
    if (!g_server.initialized)
    {
        return;
    }

    // Stop cleanup timer first
    stop_cleanup_timer();

    // Cleanup router system internally
    router_cleanup();

    // Process remaining cleanup events
    while (uv_loop_alive(g_server.loop))
    {
        if (uv_run(g_server.loop, UV_RUN_ONCE) == 0)
        {
            break;
        }
    }

    uv_loop_close(g_server.loop);

    // Final cleanup
    if (g_server.server && !g_server.server_closed)
    {
        free(g_server.server);
    }

    memset(&g_server, 0, sizeof(g_server));
    printf("Server cleanup completed\n");
}

// ============================================================================
// CONFIGURATION API
// ============================================================================

void error_hook(error_callback_t callback)
{
    g_server.error_callback = callback;
}

void shutdown_hook(shutdown_callback_t callback)
{
    g_server.shutdown_callback = callback;
}

// ============================================================================
// STATUS API
// ============================================================================

int server_is_running(void)
{
    return g_server.running;
}

int get_active_connections(void)
{
    return g_server.active_connections;
}

uv_loop_t *get_loop(void)
{
    return g_server.loop;
}

// ============================================================================
// ASYNC UTILITIES API
// ============================================================================

static void timer_callback(uv_timer_t *handle)
{
    timer_data_t *data = (timer_data_t *)handle->data;

    if (data && data->callback)
    {
        data->callback(data->user_data);
    }

    // Cleanup timeout (not interval)
    if (data && !data->is_interval)
    {
        uv_timer_stop(handle);
        uv_close((uv_handle_t *)handle, (uv_close_cb)free);
        free(data);
    }
}

uv_timer_t *set_timeout(timer_callback_t callback, uint64_t delay_ms, void *user_data)
{
    if (!g_server.initialized || !callback)
    {
        return NULL;
    }

    uv_timer_t *timer = malloc(sizeof(uv_timer_t));
    timer_data_t *data = malloc(sizeof(timer_data_t));

    if (!timer || !data)
    {
        free(timer);
        free(data);
        return NULL;
    }

    data->callback = callback;
    data->user_data = user_data;
    data->is_interval = 0;

    if (uv_timer_init(g_server.loop, timer) != 0)
    {
        free(timer);
        free(data);
        return NULL;
    }

    timer->data = data;

    if (uv_timer_start(timer, timer_callback, delay_ms, 0) != 0)
    {
        free(timer);
        free(data);
        return NULL;
    }

    return timer;
}

uv_timer_t *set_interval(timer_callback_t callback, uint64_t interval_ms, void *user_data)
{
    if (!g_server.initialized || !callback)
    {
        return NULL;
    }

    uv_timer_t *timer = malloc(sizeof(uv_timer_t));
    timer_data_t *data = malloc(sizeof(timer_data_t));

    if (!timer || !data)
    {
        free(timer);
        free(data);
        return NULL;
    }

    data->callback = callback;
    data->user_data = user_data;
    data->is_interval = 1;

    if (uv_timer_init(g_server.loop, timer) != 0)
    {
        free(timer);
        free(data);
        return NULL;
    }

    timer->data = data;

    if (uv_timer_start(timer, timer_callback, interval_ms, interval_ms) != 0)
    {
        free(timer);
        free(data);
        return NULL;
    }

    return timer;
}

void clear_timer(uv_timer_t *timer)
{
    if (!timer)
        return;

    uv_timer_stop(timer);

    timer_data_t *data = (timer_data_t *)timer->data;
    if (data)
    {
        free(data);
    }

    uv_close((uv_handle_t *)timer, (uv_close_cb)free);
}

// ============================================================================
// CONNECTION HANDLING
// ============================================================================

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    (void)suggested_size;
    client_t *client = (client_t *)handle->data;

    if (!client || client->closing || g_server.shutdown_requested)
    {
        buf->base = NULL;
        buf->len = 0;
        return;
    }

    *buf = client->read_buf;
}

static void on_client_closed(uv_handle_t *handle)
{
    client_t *client = (client_t *)handle->data;
    if (client)
    {
        remove_client_from_list(client);
        g_server.active_connections--;
        free(client);
    }
}

static void close_client(client_t *client)
{
    if (!client || client->closing)
    {
        return;
    }

    client->closing = 1;
    uv_read_stop((uv_stream_t *)&client->handle);

    if (!uv_is_closing((uv_handle_t *)&client->handle))
    {
        uv_close((uv_handle_t *)&client->handle, on_client_closed);
    }
}

static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    client_t *client = (client_t *)stream->data;

    if (!client || client->closing)
    {
        return;
    }

    if (g_server.shutdown_requested)
    {
        close_client(client);
        return;
    }

    if (nread < 0)
    {
        close_client(client);
        return;
    }

    if (nread == 0)
    {
        return; // EAGAIN/EWOULDBLOCK
    }

    // Update activity time
    client->last_activity = time(NULL);

    // Process through router
    if (buf && buf->base)
    {
        int should_close = router(&client->handle, buf->base, (size_t)nread);

        if (should_close)
        {
            close_client(client);
        }
        else
        {
            // Keep-alive: mark as keep-alive enabled
            client->keep_alive_enabled = 1;
        }
    }
}

static void on_connection(uv_stream_t *server, int status)
{
    (void)server;

    if (status < 0)
    {
        if (g_server.error_callback)
        {
            g_server.error_callback("Connection error");
        }
        return;
    }

    if (g_server.shutdown_requested)
    {
        return;
    }

    if (g_server.active_connections >= MAX_CONNECTIONS)
    {
        if (g_server.error_callback)
        {
            g_server.error_callback("Max connections reached");
        }
        return;
    }

    client_t *client = calloc(1, sizeof(client_t));
    if (!client)
    {
        return;
    }

    // Initialize client
    client->last_activity = time(NULL);
    client->keep_alive_enabled = 0;
    client->next = NULL;

    if (uv_tcp_init(g_server.loop, &client->handle) != 0)
    {
        free(client);
        return;
    }

    client->handle.data = client;
    client->read_buf = uv_buf_init(client->buffer, READ_BUFFER_SIZE);

    if (uv_accept(server, (uv_stream_t *)&client->handle) == 0)
    {
        // Enable TCP_NODELAY for better latency
        uv_tcp_nodelay(&client->handle, 1);

        if (uv_read_start((uv_stream_t *)&client->handle, alloc_buffer, on_read) == 0)
        {
            add_client_to_list(client);
            g_server.active_connections++;
        }
        else
        {
            close_client(client);
        }
    }
    else
    {
        close_client(client);
    }
}

// ============================================================================
// SHUTDOWN HANDLING
// ============================================================================

static void close_walk_cb(uv_handle_t *handle, void *arg)
{
    (void)arg;

    if (uv_is_closing(handle))
    {
        return;
    }

    if (handle->type == UV_TCP && handle != (uv_handle_t *)g_server.server)
    {
        // Client connection
        client_t *client = (client_t *)handle->data;
        if (client)
        {
            close_client(client);
        }
    }
    else if (handle->type == UV_TIMER && handle != (uv_handle_t *)g_server.cleanup_timer)
    {
        // Application timer (not our cleanup timer)
        uv_timer_stop((uv_timer_t *)handle);

        timer_data_t *data = (timer_data_t *)handle->data;
        if (data)
        {
            free(data);
        }

        uv_close(handle, (uv_close_cb)free);
    }
}

static void on_server_closed(uv_handle_t *handle)
{
    (void)handle;
    if (g_server.server)
    {
        free(g_server.server);
        g_server.server = NULL;
        g_server.server_closed = 1;
    }
}

static void on_signal(uv_signal_t *handle, int signum)
{
    (void)handle;
    const char *signal_name = (signum == SIGINT) ? "SIGINT" : "SIGTERM";
    printf("Received %s, shutting down...\n", signal_name);
    server_shutdown();
}

// ============================================================================
// ROUTER
// ============================================================================

route_trie_t *global_route_trie = NULL;

// Initialize router with default capacity
static int router_init(void)
{
    if (global_route_trie)
    {
        return 0; // Already initialized
    }

    global_route_trie = route_trie_create();
    if (!global_route_trie)
    {
        fprintf(stderr, "Error: Failed to create route trie\n");
        return 1;
    }

    return 0;
}

static void router_cleanup(void)
{
    if (global_route_trie)
    {
        // Middleware contexts will be cleaned up in route_trie_free
        route_trie_free(global_route_trie);
        global_route_trie = NULL;
    }

    reset_middleware();
}
