#ifndef HANDLERS_H
#define HANDLERS_H
#include <winsock2.h>

void handle_root(SOCKET client_socket, const char *body);
void handle_user(SOCKET client_socket, const char *body);
void handle_post_echo(SOCKET client_socket, const char *body);

#endif
