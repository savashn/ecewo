#ifndef COOKIE_H
#define COOKIE_H

#include "request.h"
#include "router.h"

char *handle_get_cookie(request_t *headers, const char *name);
void handle_set_cookie(Res *res, const char *name, const char *value, int max_age);

#define get_cookie(name) handle_get_cookie(&req->headers, name)
#define set_cookie(name, value, max_age) handle_set_cookie(res, name, value, max_age)

#endif
