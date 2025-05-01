#ifndef ECEWO_H
#define ECEWO_H

#include "router.h"

#define MAX_ROUTES 100

extern Router routes[MAX_ROUTES];
extern int route_count;

void get(const char *path, RequestHandler handler);
void post(const char *path, RequestHandler handler);
void put(const char *path, RequestHandler handler);
void del(const char *path, RequestHandler handler);

#endif
