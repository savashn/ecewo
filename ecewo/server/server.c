#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "router.h"

// Allocation callback for buffer
void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    (void)handle;
    *buf = uv_buf_init((char *)malloc(suggested_size), (unsigned int)suggested_size);
}

// Called after write is completed
void on_write_end(uv_write_t *req, int status)
{
    if (status)
    {
        fprintf(stderr, "Write error: %s\n", uv_strerror(status));
    }

    write_req_t *wr = (write_req_t *)req;
    free(wr->buf.base);
    free(wr);
}

// Called after the client socket is closed
void on_client_closed(uv_handle_t *handle)
{
    free(handle);
    printf("Connection closed successfully\n");
}

// Called when data is received
void on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
{
    if (nread < 0)
    {
        if (nread != UV_EOF)
        {
            fprintf(stderr, "Read error: %s\n", uv_strerror((int)nread));
        }
        uv_close((uv_handle_t *)client, on_client_closed);
        free(buf->base);
        return;
    }

    if (nread == 0)
    {
        free(buf->base);
        return;
    }

    // Ensure null termination
    char *data = malloc((size_t)nread + 1);
    memcpy(data, buf->base, (size_t)nread);
    data[(size_t)nread] = '\0';

    // Process request and determine if connection should be closed
    // int should_close = router((uv_tcp_t *)client, data);
    int should_close = router((uv_tcp_t *)client, data, (size_t)nread);

    free(data);
    free(buf->base);

    if (should_close)
    {
        uv_close((uv_handle_t *)client, on_client_closed);
    }
}

// Called when a new connection is received
void on_new_connection(uv_stream_t *server, int status)
{
    if (status < 0)
    {
        fprintf(stderr, "New connection error: %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t *client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(uv_default_loop(), client);

    if (uv_accept(server, (uv_stream_t *)client) == 0)
    {
        // Set TCP_NODELAY to improve response time
        int enable = 1;
        uv_tcp_nodelay(client, enable);

        // Start reading from client
        uv_read_start((uv_stream_t *)client, alloc_buffer, on_read);
    }
    else
    {
        uv_close((uv_handle_t *)client, on_client_closed);
    }
}

void ecewo(unsigned short PORT)
{
    uv_tcp_t server;
    struct sockaddr_in addr;

    // Initialize the event loop
    uv_loop_t *loop = uv_default_loop();

    // Initialize the TCP server
    uv_tcp_init(loop, &server);

    // Set SO_REUSEADDR
    uv_tcp_simultaneous_accepts(&server, 1);

    // Bind to address
    uv_ip4_addr("0.0.0.0", PORT, &addr);
    uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0);

    // Start listening
    int result = uv_listen((uv_stream_t *)&server, 128, on_new_connection);
    if (result)
    {
        fprintf(stderr, "Listen error: %s\n", uv_strerror(result));
        return;
    }

    printf("ecewo v0.21.0\n");
    printf("Server is running at: http://localhost:%d\n", PORT);

    // Run the event loop
    uv_run(loop, UV_RUN_DEFAULT);

    // Clean up
    uv_loop_close(loop);
}
