#ifndef ROUTES_H
#define ROUTES_H
#include "handlers.h"
#include "../chttp/router.h"

Route routes[] = {
    {"GET", "/", handle_root},
    {"GET", "/user", handle_user},
    {"POST", "/echo", handle_post_echo},
};

#endif
