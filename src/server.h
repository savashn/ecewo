#ifndef ECEWO_SERVER_H
#define ECEWO_SERVER_H

#include "ecewo.h"
#include "http.h"
#include "uv.h"
#include "llhttp.h"

#ifndef READ_BUFFER_SIZE
#define READ_BUFFER_SIZE 16384
#endif

struct client_s
{
    uv_tcp_t handle;
    uv_buf_t read_buf;
    char buffer[READ_BUFFER_SIZE];
    bool closing;
    uint64_t last_activity;
    bool keep_alive_enabled;
    struct client_s *next;

    Arena *connection_arena; // Lives for the duration of the connection

    // Connection-scoped parser and context
    llhttp_t persistent_parser;
    llhttp_settings_t persistent_settings;
    http_context_t persistent_context; // Lives in connection_arena
    bool parser_initialized;
    bool request_in_progress;  // True while parsing a multi-packet request

    bool taken_over;
    void *takeover_user_data;
};

#endif
