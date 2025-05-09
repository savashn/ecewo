#ifndef ROUTER_H
#define ROUTER_H

#include <stdio.h>
#include <stdarg.h>
#include "handler.h"
#include "middleware.h"
#include "compat.h"

#define MAX_ROUTES 100

extern Router routes[MAX_ROUTES];
extern int route_count;

void get(const char *path, ...);
void post(const char *path, ...);
void put(const char *path, ...);
void del(const char *path, ...);

#endif
