#include <stdio.h>
#include "handlers.h"
#include "../chttp/utils.h"

void handle_root(SOCKET client_socket, const char *body)
{
    res(client_socket, "200 OK", "application/json", "{\"message\": \"Ana sayfa\"}");
}

void handle_user(SOCKET client_socket, const char *body)
{
    res(client_socket, "200 OK", "application/json", "{\"id\": 1, \"name\": \"Ahmet\"}");
}

void handle_post_echo(SOCKET client_socket, const char *body)
{
    char json[4096];
    snprintf(json, sizeof(json), "{\"echo\": \"%s\"}", body);
    res(client_socket, "200 OK", "application/json", json);
}
