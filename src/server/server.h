#ifndef ECEWO_SERVER_H
#define ECEWO_SERVER_H

#include "ecewo.h"
#include "request.h"
#include "uv.h"
#include "llhttp.h"
#include <stdbool.h>
#include <time.h>

#define READ_BUFFER_SIZE 16384
#define MAX_CONNECTIONS 10000
#define LISTEN_BACKLOG 511
#define IDLE_TIMEOUT_SECONDS 120
#define CLEANUP_INTERVAL_MS 60000

typedef struct client_s
{
    uv_tcp_t handle;
    uv_buf_t read_buf;
    char buffer[READ_BUFFER_SIZE];
    bool closing;
    time_t last_activity;
    bool keep_alive_enabled;
    struct client_s *next;

    Arena *connection_arena; // Lives for the duration of the connection

    // Connection-scoped parser and context
    llhttp_t persistent_parser;
    llhttp_settings_t persistent_settings;
    http_context_t persistent_context; // Lives in connection_arena
    bool parser_initialized;
} client_t;

#endif
