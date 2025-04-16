#ifndef UTILS_H
#define UTILS_H

#include <winsock2.h>
#include "router.h"

void reply(Res *res, const char *status, const char *content_type, const char *body);

#endif
