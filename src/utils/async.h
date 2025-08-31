#ifndef ECEWO_ASYNC_H
#define ECEWO_ASYNC_H

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
};

static void _async_work_cb(uv_work_t *req);
static void _async_after_work_cb(uv_work_t *req, int status);

// Task result functions
void ok(async_t *task);
void fail(async_t *task, const char *error_msg);

// Task creation
int task(
    void *context,
    async_work_fn_t work_fn,
    async_response_handler_t handler);

// Task chaining
void then(
    void *context,
    int success,
    char *error,
    async_work_fn_t next_work_fn,
    async_response_handler_t handler);

#endif
