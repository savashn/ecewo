#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "compat.h"
#include "async.h"

// Thread pool work callback
static void _async_work_cb(uv_work_t *req)
{
    async_t *task = (async_t *)req->data;
    task->work_fn(task, task->context);
}

// Completion callback after thread work is done
static void _async_after_work_cb(uv_work_t *req, int status)
{
    async_t *task = (async_t *)req->data;
    task->completed = 1;

    // Store status code from libuv if there was an issue
    if (status < 0)
    {
        if (task->error)
            free(task->error);
        char error_buf[128];
        snprintf(error_buf, sizeof(error_buf), "libuv error: %s", uv_strerror(status));
        task->error = strdup(error_buf);
        task->result = 0;
    }

    // Call the response handler with result
    if (task->handler)
    {
        task->handler(task->context, task->result, task->error);
    }

    // Free error message if any
    if (task->error)
    {
        free(task->error);
        task->error = NULL;
    }

    // Free the task itself
    free(task);
}

// Mark task as successfully completed
void ok(async_t *task)
{
    if (!task)
        return;
    task->result = 1;
}

// Mark task as failed with an error message
void fail(async_t *task, const char *error_msg)
{
    if (!task)
        return;
    task->result = 0;

    if (task->error)
    {
        free(task->error);
    }

    if (error_msg)
        task->error = strdup(error_msg);
    else
        task->error = strdup("Unknown error");
}

// Creates and executes an async task
int async_execute(
    void *context,                    // User context to pass to callbacks
    async_work_fn_t work_fn,          // Function to execute in the thread pool
    async_response_handler_t handler) // Response handler called after task completion
{
    if (!work_fn)
        return -1;

    // Create task
    async_t *task = (async_t *)malloc(sizeof(async_t));
    if (!task)
    {
        fprintf(stderr, "Failed to allocate memory for async task\n");
        return -1;
    }

    // Initialize task
    task->work.data = task;
    task->context = context;
    task->completed = 0;
    task->result = 0;
    task->error = NULL;
    task->work_fn = work_fn;
    task->handler = handler;

    // Queue work
    int result = uv_queue_work(
        uv_default_loop(),
        &task->work,
        _async_work_cb,
        _async_after_work_cb);

    if (result != 0)
    {
        fprintf(stderr, "Failed to queue async work: %s\n", uv_strerror(result));
        free(task);
    }

    return result;
}

// Chains another async task after a successful response
void await_execute(
    void *context,                    // User context
    int success,                      // Whether previous task was successful
    char *error,                      // Error message if previous task failed
    async_work_fn_t next_work_fn,     // Next work function to execute if successful
    async_response_handler_t handler) // Response handler for the next task
{
    if (success)
    {
        // Previous task was successful, chain the next task
        async_execute(context, next_work_fn, handler);
    }
    else
    {
        // Previous task failed, call the handler with failure
        if (handler)
        {
            handler(context, 0, error);
        }
    }
}
