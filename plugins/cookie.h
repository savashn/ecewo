#ifndef COOKIE_H
#define COOKIE_H

#include "request.h"
#include "router.h"

char *get_cookie(request_t *headers, const char *name);

void set_cookie(Res *res, const char *name, const char *value, int max_age);

#endif
