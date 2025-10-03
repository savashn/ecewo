#ifndef ECEWO_TASK_H
#define ECEWO_TASK_H

#include "uv.h"
#include "../../vendors/arena.h"

// Opaque task handle type
typedef struct task_s Task;

// Common result handler type
typedef void (*result_handler_t)(void *context, char *error);

// Task work function type
typedef void (*work_handler_t)(Task *task, void *context);

// Internal task structure
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

// Internal task creation function
int task(
    Arena *arena,                // Arena for memory allocation
    void *context,               // User context
    work_handler_t work_fn,      // Function to execute in the thread pool
    result_handler_t result_fn); // Result handler

#define worker(context, work_fn, handler) \
    task((context)->res->arena, (context), (work_fn), (result_fn))

#endif
