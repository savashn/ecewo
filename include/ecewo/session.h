#ifndef ECEWO_SESSION_H
#define ECEWO_SESSION_H

#include <time.h>
#include <stdint.h>
#include "ecewo.h"
#include "ecewo/cookie.h"
#include "ecewo-config.h"

#ifndef ECEWO_HAS_SESSION
#error "Session integration module is not enabled. Build with -DECEWO_SESSION=ON"
#endif

#ifndef ECEWO_HAS_COOKIE
#error "Cookie module is not enabled. Build with -DECEWO_COOKIE=ON"
#endif

#define SESSION_ID_LEN 32
#define MAX_SESSIONS_DEFAULT 10

typedef struct
{
    char id[SESSION_ID_LEN + 1];
    char *data;
    time_t expires;
} Session;

int session_init(void);

void session_cleanup(void);

Session *session_create(int max_age);

Session *session_find(const char *id);

void session_value_set(Session *sess, const char *key, const char *value);

char *session_value_get(Session *sess, const char *key);

void session_value_remove(Session *sess, const char *key);

void session_free(Session *sess);

Session *session_get(Req *req);

void session_send(Res *res, Session *sess, Cookie *options);

void session_destroy(Res *res, Session *sess, Cookie *options);

void session_print_all(void);

#endif
