#ifndef ROUTES_H
#define ROUTES_H

#include "ecewo/router.h"
#include "handlers.h"

Router routes[] = {
    {"GET", "/", hello_world},
};

#endif
