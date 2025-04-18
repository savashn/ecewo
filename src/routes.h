#ifndef ROUTES_H
#define ROUTES_H
#include "handlers.h"
#include "chttp/router.h"

Router routes[] = {
    {"GET", "/test-query", handle_query},
    {"GET", "/test-slug/:slug", handle_params},
    {"GET", "/test/:slug/id/:id", handle_params_and_query},
    {"GET", "/user/:slug", get_user_by_params},
    {"GET", "/users", get_all_users},
    {"POST", "/user", add_user},
    {"GET", "/", handle_root},
};

#endif
