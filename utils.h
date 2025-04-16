#ifndef UTILS_H
#define UTILS_H

#include <winsock2.h>

void send_response(SOCKET client_socket, const char *status, const char *content_type, const char *body);

#endif
