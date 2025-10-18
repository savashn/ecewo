#include "ecewo.h"
#include "mock.h"
#include "uv.h"

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

static uv_thread_t server_thread;

void free_response(http_response_t *resp)
{
    if (resp && resp->body)
    {
        free(resp->body);
        resp->body = NULL;
    }
}

http_response_t http_request(const char *method,
                             const char *path,
                             const char *body, 
                             const char *headers)
{
    http_response_t response = {0};

#ifdef _WIN32
    static bool wsa_initialized = false;
    if (!wsa_initialized)
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            response.status_code = -1;
            return response;
        }
        wsa_initialized = true;
    }
#endif

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        response.status_code = -1;
        return response;
    }

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TEST_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        response.status_code = -1;
        return response;
    }

    // Build HTTP request
    char request[4096];
    int len;

    if (body && strlen(body) > 0)
    {
        len = snprintf(request, sizeof(request),
                       "%s %s HTTP/1.1\r\n"
                       "Host: localhost:%d\r\n"
                       "Content-Length: %zu\r\n"
                       "%s"
                       "\r\n"
                       "%s",
                       method, path, TEST_PORT, strlen(body),
                       headers ? headers : "",
                       body);
    }
    else
    {
        len = snprintf(request, sizeof(request),
                       "%s %s HTTP/1.1\r\n"
                       "Host: localhost:%d\r\n"
                       "%s"
                       "\r\n",
                       method, path, TEST_PORT,
                       headers ? headers : "");
    }

    if (send(sock, request, len, 0) < 0)
    {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        response.status_code = -1;
        return response;
    }

    // Read response
    char buffer[8192] = {0};
    int total_received = 0;
    int received;

    while (total_received < (int)sizeof(buffer) - 1)
    {
        received = recv(sock, buffer + total_received,
                       sizeof(buffer) - total_received - 1, 0);
        if (received <= 0)
            break;
        total_received += received;
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif

    if (total_received <= 0)
    {
        response.status_code = -1;
        return response;
    }

    // Parse status code
    if (sscanf(buffer, "HTTP/1.1 %d", &response.status_code) != 1)
    {
        response.status_code = -1;
        return response;
    }

    // Find body (after \r\n\r\n)
    char *body_start = strstr(buffer, "\r\n\r\n");
    if (body_start)
    {
        body_start += 4;
        response.body_len = strlen(body_start);
        response.body = malloc(response.body_len + 1);
        if (response.body)
        {
            strcpy(response.body, body_start);
        }
    }

    return response;
}
