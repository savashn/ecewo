#ifndef ROUTES_H
#define ROUTES_H
#include "handlers.h"
#include "chttp/router.h"

Route routes[] = {
    {"GET", "/", handle_root},
    {"GET", "/user", handle_user},
    {"POST", "/echo", handle_post_echo},
    {"POST", "/user", handle_create_user},
    {"GET", "/deneme", handle_query},
    {"GET", "/deneme/:slug/id/:id", handle_params_and_query},
    {"GET", "/deneme/:slug", handle_params},
};

#endif
