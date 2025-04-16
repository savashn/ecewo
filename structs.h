#ifndef STRUCTS_H
#define STRUCTS_H
#include "router.h"

typedef struct
{
    const char *method;
    const char *path;
    RequestHandler handler;
} Route;

#endif
