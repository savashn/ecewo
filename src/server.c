#include <stdlib.h>
#include <signal.h>
#include <inttypes.h>
#include "server.h"
#include "route-trie.h"
#include "request.h"
#include "middleware.h"
#include "arena.h"

typedef struct timer_data_s
{
    timer_callback_t callback;
    void *user_data;
    bool is_interval;
} timer_data_t;

static struct
{
    bool initialized;
    bool running;
    bool shutdown_requested;
    int active_connections;
    int pending_async_work;

    uv_loop_t *loop;
    uv_tcp_t *server;
    uv_signal_t sigint_handle;
    uv_signal_t sigterm_handle;
    uv_async_t shutdown_async;

    shutdown_callback_t shutdown_callback;

    client_t *client_list_head;
    uv_timer_t *cleanup_timer;

    bool server_closed;
} g_server = {0};

route_trie_t *global_route_trie = NULL;

static void add_client_to_list(client_t *client)
{
    client->next = g_server.client_list_head;
    g_server.client_list_head = client;
}

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
        current->next = client->next;
}

static void on_client_closed(uv_handle_t *handle)
{
    client_t *client = (client_t *)handle->data;
    if (client)
    {
        remove_client_from_list(client);
        g_server.active_connections--;

        if (client->connection_arena)
        {
            arena_free(client->connection_arena);
            free(client->connection_arena);
        }

        free(client);
    }
}

static void close_client(client_t *client)
{
    if (!client || client->closing)
        return;

    client->closing = true;
    uv_read_stop((uv_stream_t *)&client->handle);

    if (!uv_is_closing((uv_handle_t *)&client->handle))
        uv_close((uv_handle_t *)&client->handle, on_client_closed);
}

static void cleanup_idle_connections(uv_timer_t *handle)
{
    (void)handle;

    if (g_server.shutdown_requested)
        return;

    time_t now = time(NULL);
    client_t *current = g_server.client_list_head;
    int checked = 0;

    while (current)
    {
        client_t *next = current->next;
        checked++;

        if (current->keep_alive_enabled && !current->closing)
        {
            time_t idle_time = now - current->last_activity;
            if (idle_time > IDLE_TIMEOUT_SECONDS)
                close_client(current);
        }
        current = next;
    }
}

static int start_cleanup_timer(void)
{
    g_server.cleanup_timer = malloc(sizeof(uv_timer_t));
    if (!g_server.cleanup_timer)
        return -1;

    if (uv_timer_init(g_server.loop, g_server.cleanup_timer) != 0)
    {
        free(g_server.cleanup_timer);
        g_server.cleanup_timer = NULL;
        return -1;
    }

    if (uv_timer_start(g_server.cleanup_timer, cleanup_idle_connections,
                       CLEANUP_INTERVAL_MS, CLEANUP_INTERVAL_MS) != 0)
    {
        uv_close((uv_handle_t *)g_server.cleanup_timer, (uv_close_cb)free);
        g_server.cleanup_timer = NULL;
        return -1;
    }

    return 0;
}

static void stop_cleanup_timer(void)
{
    if (g_server.cleanup_timer)
    {
        uv_timer_stop(g_server.cleanup_timer);
        uv_close((uv_handle_t *)g_server.cleanup_timer, (uv_close_cb)free);
        g_server.cleanup_timer = NULL;
    }
}

void increment_async_work(void)
{
    g_server.pending_async_work++;
}

void decrement_async_work(void)
{
    if (g_server.pending_async_work > 0)
        g_server.pending_async_work--;
}

int get_pending_async_work(void)
{
    return g_server.pending_async_work;
}

static int client_connection_init(client_t *client)
{
    if (!client)
        return -1;

    client->connection_arena = calloc(1, sizeof(Arena));
    if (!client->connection_arena)
        return -1;

    http_context_t *ctx = arena_alloc(client->connection_arena, sizeof(http_context_t));
    if (!ctx)
    {
        arena_free(client->connection_arena);
        free(client->connection_arena);
        client->connection_arena = NULL;
        return -1;
    }

    memcpy(&client->persistent_context, ctx, sizeof(http_context_t));
    return 0;
}

static void client_parser_init(client_t *client)
{
    if (!client || client->parser_initialized)
        return;

    llhttp_settings_init(&client->persistent_settings);

    client->persistent_settings.on_url = on_url_cb;
    client->persistent_settings.on_header_field = on_header_field_cb;
    client->persistent_settings.on_header_value = on_header_value_cb;
    client->persistent_settings.on_method = on_method_cb;
    client->persistent_settings.on_body = on_body_cb;
    client->persistent_settings.on_headers_complete = on_headers_complete_cb;
    client->persistent_settings.on_message_complete = on_message_complete_cb;

    llhttp_init(&client->persistent_parser, HTTP_REQUEST, &client->persistent_settings);

    llhttp_set_lenient_headers(&client->persistent_parser, 0);
    llhttp_set_lenient_chunked_length(&client->persistent_parser, 0);
    llhttp_set_lenient_keep_alive(&client->persistent_parser, 0);

    client->parser_initialized = true;
}

static void client_context_init(client_t *client)
{
    if (!client || !client->connection_arena)
        return;

    http_context_t *ctx = &client->persistent_context;

    memset(ctx, 0, sizeof(http_context_t));
    ctx->arena = client->connection_arena;
    ctx->parser = &client->persistent_parser;
    ctx->settings = &client->persistent_settings;

    client->persistent_parser.data = ctx;

    ctx->url_capacity = 512;
    ctx->url = arena_alloc(client->connection_arena, ctx->url_capacity);
    if (ctx->url)
        ctx->url[0] = '\0';

    ctx->method_capacity = 32;
    ctx->method = arena_alloc(client->connection_arena, ctx->method_capacity);
    if (ctx->method)
        ctx->method[0] = '\0';

    ctx->header_field_capacity = 128;
    ctx->current_header_field = arena_alloc(client->connection_arena, ctx->header_field_capacity);
    if (ctx->current_header_field)
        ctx->current_header_field[0] = '\0';

    ctx->body_capacity = 1024;
    ctx->body = arena_alloc(client->connection_arena, ctx->body_capacity);
    if (ctx->body)
        ctx->body[0] = '\0';

    ctx->headers.capacity = 32;
    ctx->headers.items = arena_alloc(client->connection_arena,
                                     ctx->headers.capacity * sizeof(request_item_t));
    if (ctx->headers.items)
        memset(ctx->headers.items, 0, ctx->headers.capacity * sizeof(request_item_t));

    memset(&ctx->query_params, 0, sizeof(request_t));
    memset(&ctx->url_params, 0, sizeof(request_t));

    ctx->keep_alive = 1;
    ctx->message_complete = 0;
    ctx->headers_complete = 0;
    ctx->last_error = HPE_OK;
    ctx->error_reason = NULL;
}

static void client_context_reset(client_t *client)
{
    if (!client || !client->connection_arena)
        return;

    arena_reset(client->connection_arena);
    client_context_init(client);

    http_context_t *ctx = &client->persistent_context;

    ctx->url_length = 0;
    ctx->method_length = 0;
    ctx->body_length = 0;
    ctx->header_field_length = 0;

    ctx->headers.count = 0;
    ctx->headers.capacity = 0;
    ctx->headers.items = NULL;
    
    ctx->query_params.count = 0;
    ctx->query_params.capacity = 0;
    ctx->query_params.items = NULL;
    
    ctx->url_params.count = 0;
    ctx->url_params.capacity = 0;
    ctx->url_params.items = NULL;

    ctx->message_complete = 0;
    ctx->headers_complete = 0;
    ctx->keep_alive = 1;
    ctx->last_error = HPE_OK;
    ctx->error_reason = NULL;

    if (ctx->url)
        ctx->url[0] = '\0';
    if (ctx->method)
        ctx->method[0] = '\0';
    if (ctx->body)
        ctx->body[0] = '\0';
    if (ctx->current_header_field)
        ctx->current_header_field[0] = '\0';

    llhttp_reset(&client->persistent_parser);
}

static void close_walk_cb(uv_handle_t *handle, void *arg)
{
    (void)arg;

    if (uv_is_closing(handle))
        return;

    if (handle->type == UV_TCP)
    {
        if (handle != (uv_handle_t *)g_server.server)
        {
            client_t *client = (client_t *)handle->data;
            if (client)
                close_client(client);
        }
    }
    else if (handle->type == UV_TIMER)
    {
        if (handle != (uv_handle_t *)g_server.cleanup_timer)
        {
            uv_timer_stop((uv_timer_t *)handle);
            timer_data_t *data = (timer_data_t *)handle->data;
            if (data)
                free(data);
            uv_close(handle, (uv_close_cb)free);
        }
    }
    else if (handle->type == UV_POLL)
    {
        uv_poll_stop((uv_poll_t *)handle);
        uv_close(handle, NULL);
    }
    else if (handle->type == UV_ASYNC)
    {
        uv_close(handle, NULL);
    }
    else if (handle->type == UV_SIGNAL)
    {
        uv_signal_stop((uv_signal_t *)handle);
        uv_close(handle, NULL);
    }
    else
    {
        uv_close(handle, NULL);
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

static void server_shutdown(void)
{
    if (g_server.shutdown_requested)
        return;

    g_server.shutdown_requested = 1;
    g_server.running = 0;

    if (g_server.shutdown_callback)
        g_server.shutdown_callback();

    stop_cleanup_timer();

    if (!uv_is_closing((uv_handle_t *)&g_server.shutdown_async))
        uv_close((uv_handle_t *)&g_server.shutdown_async, NULL);

    const char *is_worker = getenv("ECEWO_WORKER");
    bool in_cluster = (is_worker && strcmp(is_worker, "1") == 0);
    
    if (!in_cluster)
    {
        if (!uv_is_closing((uv_handle_t *)&g_server.sigint_handle))
        {
            uv_signal_stop(&g_server.sigint_handle);
            uv_close((uv_handle_t *)&g_server.sigint_handle, NULL);
        }

        if (!uv_is_closing((uv_handle_t *)&g_server.sigterm_handle))
        {
            uv_signal_stop(&g_server.sigterm_handle);
            uv_close((uv_handle_t *)&g_server.sigterm_handle, NULL);
        }
    }

    if (g_server.server && !uv_is_closing((uv_handle_t *)g_server.server))
        uv_close((uv_handle_t *)g_server.server, on_server_closed);

    client_t *current = g_server.client_list_head;
    while (current)
    {
        client_t *next = current->next;
        close_client(current);
        current = next;
    }

    uint64_t start = uv_now(g_server.loop);
    
    while (get_pending_async_work() > 0)
    {
        if ((uv_now(g_server.loop) - start) >= SHUTDOWN_TIMEOUT_MS)
        {
            LOG_DEBUG("Shutdown timeout: %d operations abandoned",
                    get_pending_async_work());
            break;
        }
        
        uv_run(g_server.loop, UV_RUN_ONCE);
    }

    uv_walk(g_server.loop, close_walk_cb, NULL);

    start = uv_now(g_server.loop);
    
    while (g_server.active_connections > 0)
    {
        if ((uv_now(g_server.loop) - start) >= SHUTDOWN_TIMEOUT_MS)
        {
            LOG_DEBUG("Shutdown timeout: %d connections force closed",
                    g_server.active_connections);
            break;
        }
        
        uv_run(g_server.loop, UV_RUN_ONCE);
    }
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

static void server_cleanup(void)
{
    if (!g_server.initialized)
        return;

    if (!g_server.shutdown_requested)
        server_shutdown();

    stop_cleanup_timer();
    router_cleanup();

    uint64_t start = uv_now(g_server.loop);
    
    while (uv_loop_alive(g_server.loop))
    {
        if ((uv_now(g_server.loop) - start) >= CLEANUP_TIMEOUT_MS)
        {
            LOG_DEBUG("Cleanup timeout: forcing loop close");
            break;
        }
        
        uv_run(g_server.loop, UV_RUN_NOWAIT);
    }

    int result = uv_loop_close(g_server.loop);
    if (result != 0)
    {
        uv_walk(g_server.loop, close_walk_cb, NULL);
        uv_run(g_server.loop, UV_RUN_NOWAIT);
        uv_loop_close(g_server.loop);
    }

    if (g_server.server && !g_server.server_closed)
        free(g_server.server);
    
    memset(&g_server, 0, sizeof(g_server));
}

static void on_async_shutdown(uv_async_t *handle)
{
    (void)handle;
    server_shutdown();
}

static void on_signal(uv_signal_t *handle, int signum)
{
    (void)handle;

    if (g_server.shutdown_requested)
        return;

    const char *signal_name = (signum == SIGINT) ? "SIGINT" : "SIGTERM";
    LOG_DEBUG("Received %s, shutting down...", signal_name);
    
    uv_async_send(&g_server.shutdown_async);
}

static int router_init(void)
{
    if (global_route_trie)
        return 0;

    global_route_trie = route_trie_create();
    if (!global_route_trie)
    {
        LOG_ERROR("Failed to create route trie");
        return 1;
    }

    return 0;
}

int server_init(void)
{
    if (g_server.initialized)
        return SERVER_ALREADY_INITIALIZED;

    memset(&g_server, 0, sizeof(g_server));

    g_server.loop = uv_default_loop();
    if (!g_server.loop)
        return SERVER_INIT_FAILED;

    const char *is_worker = getenv("ECEWO_WORKER");
    bool in_cluster = (is_worker && strcmp(is_worker, "1") == 0);
    
    if (!in_cluster)
    {
        if (uv_signal_init(g_server.loop, &g_server.sigint_handle) != 0 ||
            uv_signal_init(g_server.loop, &g_server.sigterm_handle) != 0)
        {
            return SERVER_INIT_FAILED;
        }

        uv_signal_start(&g_server.sigint_handle, on_signal, SIGINT);
        uv_signal_start(&g_server.sigterm_handle, on_signal, SIGTERM);
    }

    if (uv_async_init(g_server.loop, &g_server.shutdown_async, on_async_shutdown) != 0)
        return SERVER_INIT_FAILED;

    if (router_init() != 0)
        return SERVER_INIT_FAILED;

    g_server.initialized = 1;
    atexit(server_cleanup);

    return SERVER_OK;
}

static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    client_t *client = (client_t *)stream->data;

    if (!client || client->closing)
        return;

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
        return; // EAGAIN/EWOULDBLOCK

    client->last_activity = time(NULL);

    if (!client->parser_initialized)
    {
        client_parser_init(client);
        client_context_init(client);
    }
    else
    {
        client_context_reset(client);
    }

    if (buf && buf->base)
    {
        int should_close = router(client, buf->base, (size_t)nread);

        if (should_close)
        {
            close_client(client);
        }
        else
        {
            client->keep_alive_enabled = true;
        }
    }
}

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

static void on_connection(uv_stream_t *server, int status)
{
    (void)server;

    if (status < 0)
    {
        LOG_ERROR("Connection error");
        return;
    }

    if (g_server.shutdown_requested)
        return;

    if (g_server.active_connections >= MAX_CONNECTIONS)
    {
        LOG_DEBUG("Max connections (%d) reached", MAX_CONNECTIONS);
        return;
    }

    client_t *client = calloc(1, sizeof(client_t));
    if (!client)
        return;

    client->last_activity = time(NULL);
    client->keep_alive_enabled = false;
    client->next = NULL;
    client->parser_initialized = false;
    client->connection_arena = NULL;

    if (client_connection_init(client) != 0)
    {
        free(client);
        return;
    }

    if (uv_tcp_init(g_server.loop, &client->handle) != 0)
    {
        if (client->connection_arena)
        {
            arena_free(client->connection_arena);
            free(client->connection_arena);
        }
        free(client);
        return;
    }

    client->handle.data = client;
    client->read_buf = uv_buf_init(client->buffer, READ_BUFFER_SIZE);

    if (uv_accept(server, (uv_stream_t *)&client->handle) == 0)
    {
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

int server_listen(uint16_t port)
{
    if (port == 0)
    {
        LOG_ERROR("Invalid port %" PRIu16 " (must be 1-65535)", port);
        return SERVER_INVALID_PORT;
    }

    if (!g_server.initialized)
        return SERVER_NOT_INITIALIZED;

    if (g_server.running)
        return SERVER_ALREADY_RUNNING;

    g_server.server = malloc(sizeof(uv_tcp_t));
    if (!g_server.server)
        return SERVER_OUT_OF_MEMORY;

    if (uv_tcp_init(g_server.loop, g_server.server) != 0)
    {
        free(g_server.server);
        g_server.server = NULL;
        return SERVER_INIT_FAILED;
    }

    uv_tcp_simultaneous_accepts(g_server.server, 1);

    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", port, &addr);

#ifdef _WIN32
    unsigned int flags = 0;
#else
    unsigned int flags = UV_TCP_REUSEPORT;
#endif
    
    if (uv_tcp_bind(g_server.server, (const struct sockaddr *)&addr, flags) != 0)
    {
        free(g_server.server);
        g_server.server = NULL;
        LOG_ERROR("Failed to bind to port %" PRIu16 " (may be in use)", port);
        return SERVER_BIND_FAILED;
    }

    if (uv_listen((uv_stream_t *)g_server.server, LISTEN_BACKLOG, on_connection) != 0)
    {
        free(g_server.server);
        g_server.server = NULL;
        LOG_ERROR("Failed to listen on port %" PRIu16, port);
        return SERVER_LISTEN_FAILED;
    }

    if (start_cleanup_timer() != 0)
        LOG_DEBUG("Failed to start cleanup timer");

    g_server.running = 1;
    
    const char *is_worker = getenv("ECEWO_WORKER");
    if (!is_worker || strcmp(is_worker, "1") != 0)
        printf("Server listening on http://localhost:%" PRIu16 "\n", port);
    
    return SERVER_OK;
}

void server_run(void)
{
    if (!g_server.initialized || !g_server.running)
    {
        LOG_ERROR("Server not initialized or not listening");
        return;
    }
    
    uv_run(g_server.loop, UV_RUN_DEFAULT);
    server_cleanup();
}

void shutdown_hook(shutdown_callback_t callback)
{
    g_server.shutdown_callback = callback;
}

bool server_is_running(void)
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

static void timer_callback(uv_timer_t *handle)
{
    timer_data_t *data = (timer_data_t *)handle->data;

    if (data && data->callback)
        data->callback(data->user_data);

    if (data && !data->is_interval)
    {
        uv_timer_stop(handle);
        uv_close((uv_handle_t *)handle, (uv_close_cb)free);
        free(data);
    }
}

Timer *set_timeout(timer_callback_t callback, uint64_t delay_ms, void *user_data)
{
    if (!g_server.initialized || !callback)
        return NULL;

    Timer *timer = malloc(sizeof(Timer));
    timer_data_t *data = malloc(sizeof(timer_data_t));

    if (!timer || !data)
    {
        free(timer);
        free(data);
        return NULL;
    }

    data->callback = callback;
    data->user_data = user_data;
    data->is_interval = false;

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

Timer *set_interval(timer_callback_t callback, uint64_t interval_ms, void *user_data)
{
    if (!g_server.initialized || !callback)
        return NULL;

    Timer *timer = malloc(sizeof(Timer));
    timer_data_t *data = malloc(sizeof(timer_data_t));

    if (!timer || !data)
    {
        free(timer);
        free(data);
        return NULL;
    }

    data->callback = callback;
    data->user_data = user_data;
    data->is_interval = true;

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

void clear_timer(Timer *timer)
{
    if (!timer)
        return;

    uv_timer_stop(timer);

    timer_data_t *data = (timer_data_t *)timer->data;
    if (data)
    {
        free(data);
        timer->data = NULL;
    }

    uv_close((uv_handle_t *)timer, (uv_close_cb)free);
}
