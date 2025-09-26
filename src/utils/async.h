#ifndef ECEWO_ASYNC_H
#define ECEWO_ASYNC_H

#include <stdbool.h>
#include "uv.h"
#include "../../vendors/arena.h"

// Opaque task handle type
typedef struct async_t async_t;

// Common response handler type
typedef void (*async_response_handler_t)(void *context, int success, char *error);

// Task work function type
typedef void (*async_work_fn_t)(async_t *task, void *context);

// Internal task structure
struct async_t
{
    uv_work_t work;
    void *context;  // User provided context data
    Arena *arena;   // Arena reference for error handling
    bool completed; // Flag indicating if task is completed
    bool result;    // 1 for success, 0 for failure
    char *error;    // Error message

    // Task callbacks
    async_work_fn_t work_fn;          // Work to be done in thread pool
    async_response_handler_t handler; // Response handler
};

// Task result functions
void ok(async_t *task);
void fail(async_t *task, const char *error_msg);

// Internal task creation function
int task_internal(
    Arena *arena,                      // Arena for memory allocation
    void *context,                     // User context
    async_work_fn_t work_fn,           // Function to execute in the thread pool
    async_response_handler_t handler); // Response handler

// Internal function for task chaining
void then_internal(
    Arena *arena,                      // Arena for memory allocation
    void *context,                     // Arena-allocated context
    int success,                       // Whether previous task was successful
    char *error,                       // Error message if previous task failed
    async_work_fn_t next_work_fn,      // Next work function to execute if successful
    async_response_handler_t handler); // Response handler for the next task

#define task(context, work_fn, handler) \
    task_internal((context)->res->arena, (context), (work_fn), (handler))

#define then(context, success, error, next_work_fn, handler) \
    then_internal((context)->res->arena, (context), (success), (error), (next_work_fn), (handler))

#endif
