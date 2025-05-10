#ifndef ROUTER_H
#define ROUTER_H

#include <stdio.h>
#include <stdarg.h>
#include "handler.h"
#include "middleware.h"
#include "compat.h"

extern Router *routes;
extern int route_count;
extern int routes_capacity;

void init_router(void);
void cleanup_router(void);

void get(const char *path, ...);
void post(const char *path, ...);
void put(const char *path, ...);
void del(const char *path, ...);

#endif
