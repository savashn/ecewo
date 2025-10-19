#ifndef ECEWO_TASK_H
#define ECEWO_TASK_H

#include "ecewo.h"
#include "uv.h"

struct task_s
{
    uv_work_t work; // libuv requirement
    void *context;  // User provided context data
    Arena *arena;   // Arena reference for error handling
    char *error;    // libuv error message

    // Task callbacks
    work_handler_t work_fn;     // Work to be done in thread pool
    result_handler_t result_fn; // Result handler
};

#endif
