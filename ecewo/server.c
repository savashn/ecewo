#include <stdio.h>
#include <winsock2.h>
#include "router.h"

const int PORT = 4000;
const int BUFFER_SIZE = 2048;

void ecewo()
{
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server, client;
    int client_len = sizeof(client);
    char *buffer = malloc(BUFFER_SIZE);

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printf("WSAStartup error: %d\n", WSAGetLastError());
        return;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET)
    {
        printf("Socket error: %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR)
    {
        printf("Bind error: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    listen(server_socket, 5);
    printf("ecewo v0.10.0\n");
    printf("Server is running at: http://localhost:%d\n", PORT);

    while (1)
    {
        client_socket = accept(server_socket, (struct sockaddr *)&client, &client_len);

        if (client_socket == INVALID_SOCKET)
        {
            printf("Accept error: %d\n", WSAGetLastError());
            continue;
        }

        memset(buffer, 0, BUFFER_SIZE);
        recv(client_socket, buffer, BUFFER_SIZE, 0);
        router(client_socket, buffer);
        closesocket(client_socket);
    }

    free(buffer);
    closesocket(server_socket);
    WSACleanup();
}
