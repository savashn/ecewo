#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "router.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define SOCKET_ERROR_VALUE INVALID_SOCKET
#define CLOSE_SOCKET(s) closesocket(s)
#define INIT_SOCKETS()                                                \
    {                                                                 \
        WSADATA wsa;                                                  \
        int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa);            \
        if (wsa_result != 0)                                          \
        {                                                             \
            printf("WSAStartup failed with error: %d\n", wsa_result); \
            return;                                                   \
        }                                                             \
    }
#define CLEANUP_SOCKETS() WSACleanup()
#define SOCKET_ERROR_STR() get_windows_error()
#else
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <netinet/tcp.h> // For TCP_NODELAY
typedef int socket_t;
#define SOCKET_ERROR_VALUE -1
#define CLOSE_SOCKET(s) close(s)
#define INIT_SOCKETS()
#define CLEANUP_SOCKETS()
#define SOCKET_ERROR_STR() strerror(errno)
#endif

const int BUFFER_SIZE = 4096;

#ifdef _WIN32
// Helper function to get Windows error message
char *get_windows_error()
{
    static char err_buf[256];
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&err_buf,
        sizeof(err_buf) - 1,
        NULL);
    // Remove newline characters
    char *p = err_buf;
    while (*p)
    {
        if (*p == '\r' || *p == '\n')
            *p = ' ';
        p++;
    }
    return err_buf;
}
#endif

// Handle client connection and process all requests from it
void handle_client_connection(socket_t client_socket)
{
    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer)
    {
        printf("Memory allocation failed for request buffer\n");
        return;
    }

    // Set TCP_NODELAY on client socket to improve response time
    int opt = 1;
    if (setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&opt, sizeof(opt)) < 0)
    {
        printf("Setsockopt on client socket (TCP_NODELAY) failed: %s\n", SOCKET_ERROR_STR());
        // Continue anyway
    }

    // Set receive timeout to prevent hanging
#ifdef _WIN32
    DWORD timeout = 5000; // 5 seconds in milliseconds
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) < 0)
#else
    struct timeval timeout;
    timeout.tv_sec = 5; // 5 seconds
    timeout.tv_usec = 0;
    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) < 0)
#endif
    {
        printf("Failed to set socket receive timeout: %s\n", SOCKET_ERROR_STR());
        // Continue anyway
    }

    // Process all requests on this connection
    while (1)
    {
        // Clear the buffer before receiving data
        memset(buffer, 0, BUFFER_SIZE);

        // Receive data
        int recv_size = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (recv_size > 0)
        {
            buffer[recv_size] = '\0'; // Ensure null termination

            // Process request and determine if connection should be closed
            int should_close = router(client_socket, buffer);

            if (should_close)
            {
                break; // Close connection if router indicates to do so
            }
        }
        else if (recv_size == 0)
        {
            printf("Client disconnected\n");
            break;
        }
        else
        {
            // Timeout or error occurred
            printf("Recv failed: %s\n", SOCKET_ERROR_STR());
            break;
        }
    }

    // Clean up
    free(buffer);
    CLOSE_SOCKET(client_socket);
    printf("Connection closed successfully\n");
}

void ecewo(const unsigned short PORT)
{
    socket_t server_socket;
    struct sockaddr_in server, client;
    socklen_t client_len = sizeof(client);

    // Initialize socket library (only needed for Windows)
    INIT_SOCKETS();

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == SOCKET_ERROR_VALUE)
    {
        printf("Socket creation failed: %s\n", SOCKET_ERROR_STR());
        CLEANUP_SOCKETS();
        return;
    }

    // Set socket options to allow reuse of local addresses
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) < 0)
    {
        printf("Setsockopt (SO_REUSEADDR) failed: %s\n", SOCKET_ERROR_STR());
        CLOSE_SOCKET(server_socket);
        CLEANUP_SOCKETS();
        return;
    }

    // Set TCP_NODELAY to improve response time
    if (setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&opt, sizeof(opt)) < 0)
    {
        printf("Setsockopt (TCP_NODELAY) failed: %s\n", SOCKET_ERROR_STR());
        // Continue anyway, this is just an optimization
    }

    // Prepare server address structure
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("Bind failed: %s\n", SOCKET_ERROR_STR());
        CLOSE_SOCKET(server_socket);
        CLEANUP_SOCKETS();
        return;
    }

    // Listen for connections
    if (listen(server_socket, 10) < 0)
    {
        printf("Listen failed: %s\n", SOCKET_ERROR_STR());
        CLOSE_SOCKET(server_socket);
        CLEANUP_SOCKETS();
        return;
    }

    printf("ecewo v0.14.0\n");
    printf("Server is running at: http://localhost:%d\n", PORT);

    // Main server loop
    while (1)
    {
        // Accept connection
        socket_t client_socket = accept(server_socket, (struct sockaddr *)&client, &client_len);

        if (client_socket == SOCKET_ERROR_VALUE)
        {
            printf("Accept failed: %s\n", SOCKET_ERROR_STR());
            continue;
        }

        // Handle the client connection in a separate function
        handle_client_connection(client_socket);
    }

    // Clean up
    CLOSE_SOCKET(server_socket);
    CLEANUP_SOCKETS();
}
