#ifndef ROUTER_H
#define ROUTER_H
#include <winsock2.h>

#define MAX_DYNAMIC_PARAMS 10
#define MAX_QUERY_PARAMS 20

typedef struct
{
    char *key;
    char *value;
} param_t;

typedef struct
{
    param_t *params;
    int count;
} params_t;

typedef struct
{
    const char *key;
    const char *value;
} query_item_t;

typedef struct
{
    query_item_t items[MAX_QUERY_PARAMS];
    int count;
} query_t;

typedef struct
{
    SOCKET client_socket;
    const char *method;
    const char *path;
    const char *body;
    query_t query;
    params_t params;
} Req;

typedef struct
{
    SOCKET client_socket;
    char *status;
    char *content_type;
    char *body;
} Res;

typedef void (*RequestHandler)(Req *req, Res *res);

typedef struct
{
    const char *method;
    const char *path;
    RequestHandler handler;
} Route;

void router(SOCKET client_socket, const char *request);
void reply(Res *res, const char *status, const char *content_type, const char *body);

const char *params_get(params_t *params, const char *key);
const char *query_get(query_t *query, const char *key);

#endif
