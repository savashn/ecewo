#ifndef ECEWO_H
#define ECEWO_H

#include <stdio.h>
#include <stdarg.h>
#include "router.h"
#include "middleware.h"
#include "compat.h"

extern Router *routes;
extern int route_count;
extern int routes_capacity;

void expand_routes(void);

void get(const char *path, ...);
void post(const char *path, ...);
void put(const char *path, ...);
void del(const char *path, ...);

#endif
