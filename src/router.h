#ifndef ECEWO_ROUTER_H
#define ECEWO_ROUTER_H

#include "ecewo.h"
#include "server.h"

typedef enum RouterResult {
  REQUEST_KEEP_ALIVE,
  REQUEST_CLOSE,
  REQUEST_PENDING
} RouterResult;

int router(client_t *client, const char *request_data, size_t request_len);

#endif
