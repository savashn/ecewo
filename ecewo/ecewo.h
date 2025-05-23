#ifndef ECEWO_H
#define ECEWO_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "router.h"
#include "middleware.h"
#include "compat.h"

extern Router *routes;
extern size_t route_count;
extern size_t routes_capacity;

void expand_routes(void);

void get(const char *path, ...);
void post(const char *path, ...);
void put(const char *path, ...);
void del(const char *path, ...);

#endif
