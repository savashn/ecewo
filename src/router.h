#ifndef ECEWO_ROUTER_H
#define ECEWO_ROUTER_H

#include "ecewo.h"

typedef struct client_s client_t;

int router(client_t *client, const char *request_data, size_t request_len);

#endif
