#ifndef MOCK_H
#define MOCK_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

typedef struct
{
    int status_code;
    char *body;
    size_t body_len;
} http_response_t;

#define TEST_PORT 8888

void free_response(http_response_t *resp);
http_response_t http_request(const char *method,
                             const char *path,
                             const char *body, 
                             const char *headers);

#endif
