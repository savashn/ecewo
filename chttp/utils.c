#include "utils.h"
#include <stdio.h>
#include <string.h>

void reply(Res *res, const char *status, const char *content_type, const char *body)
{
    char response[4096];

    snprintf(response, sizeof(response),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             status, content_type, strlen(body), body);
    send(res->client_socket, response, strlen(response), 0);
}
