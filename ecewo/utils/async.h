#ifndef ASYNC_H
#define ASYNC_H

#include <uv.h>

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
    void *context; // User provided context data
    int completed; // Flag indicating if task is completed
    int result;    // 1 for success, 0 for failure
    char *error;   // Error message if result is 0

    // Task callbacks
    async_work_fn_t work_fn;          // Work to be done in thread pool
    async_response_handler_t handler; // Response handler

    // Optional cleanup function for context
    void (*cleanup_fn)(void *context); // Function to free context resources
};

static void _async_work_cb(uv_work_t *req);
static void _async_after_work_cb(uv_work_t *req, int status);
void ok(async_t *task);
void fail(async_t *task, const char *error_msg);
int async_execute(
    void *context,
    async_work_fn_t work_fn,
    async_response_handler_t handler,
    void (*cleanup_fn)(void *context));

void await_execute(
    void *context,
    int success,
    char *error,
    async_work_fn_t next_work_fn,
    async_response_handler_t handler,
    void (*cleanup_fn)(void *context));

#define async(ctx, tag) \
    async_execute(      \
        ctx,            \
        tag##_work,     \
        tag##_done,     \
        free_async)

#define await(ctx, tag) \
    await_execute(      \
        ctx,            \
        success,        \
        error,          \
        tag##_work,     \
        tag##_done,     \
        free_async)

#endif
