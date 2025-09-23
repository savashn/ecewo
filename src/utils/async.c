#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../lib/router.h" // For Res definition
#include "async.h"

// Thread pool work callback
static void _async_work_cb(uv_work_t *req)
{
    async_t *task = (async_t *)req->data;
    if (task && task->work_fn)
        task->work_fn(task, task->context);
}

// Completion callback after thread work is done
static void _async_after_work_cb(uv_work_t *req, int status)
{
    async_t *task = (async_t *)req->data;
    if (!task)
        return;

    task->completed = 1;

    // Handle libuv errors
    if (status < 0)
    {
        task->result = 0;

        // Try to get arena from task for error allocation
        if (task->arena)
        {
            char error_buf[128];
            snprintf(error_buf, sizeof(error_buf), "libuv error: %s", uv_strerror(status));
            task->error = arena_strdup(task->arena, error_buf);
        }
        else
        {
            task->error = "async operation failed";
        }
    }

    // Call the response handler with result
    if (task->handler)
    {
        task->handler(task->context, task->result, task->error);
    }
}

// Mark task as successfully completed
void ok(async_t *task)
{
    if (!task)
        return;
    task->result = 1;
    task->error = NULL;
}

// Mark task as failed with an error message
void fail(async_t *task, const char *error_msg)
{
    if (!task)
        return;

    task->result = 0;

    if (error_msg)
    {
        // Use arena from task if available
        if (task->arena)
        {
            task->error = arena_strdup(task->arena, error_msg);
        }
        else
        {
            task->error = (char *)error_msg; // Fallback to direct pointer
        }
    }
    else
    {
        task->error = "Unknown error";
    }
}

// Creates and executes an async task
int task_internal(
    Arena *arena,                     // Arena for memory allocation
    void *context,                    // User context
    async_work_fn_t work_fn,          // Function to execute in the thread pool
    async_response_handler_t handler) // Response handler
{
    if (!arena || !context || !work_fn)
    {
        fprintf(stderr, "async task: arena, context and work_fn are required\n");
        return -1;
    }

    // Allocate task from provided arena
    async_t *task = arena_alloc(arena, sizeof(async_t));
    if (!task)
    {
        fprintf(stderr, "Failed to allocate memory for async task from arena\n");
        return -1;
    }

    // Initialize task
    task->work.data = task;
    task->context = context;
    task->arena = arena; // Store arena reference for error handling
    task->completed = 0;
    task->result = 0;
    task->error = NULL;
    task->work_fn = work_fn;
    task->handler = handler;

    // Queue work in libuv thread pool
    int result = uv_queue_work(
        uv_default_loop(),
        &task->work,
        _async_work_cb,
        _async_after_work_cb);

    if (result != 0)
    {
        fprintf(stderr, "Failed to queue async work: %s\n", uv_strerror(result));
        return result;
    }

    return 0;
}

// Chains another async task after a successful response
void then_internal(
    Arena *arena,                     // Arena for memory allocation
    void *context,                    // Arena-allocated context
    int success,                      // Whether previous task was successful
    char *error,                      // Error message if previous task failed
    async_work_fn_t next_work_fn,     // Next work function to execute if successful
    async_response_handler_t handler) // Response handler for the next task
{
    if (!arena || !context)
    {
        if (handler)
        {
            handler(context, 0, "Invalid arena or context for task chaining");
        }
        return;
    }

    if (success)
    {
        // Previous task was successful, chain the next task
        int result = task_internal(arena, context, next_work_fn, handler);
        if (result != 0 && handler)
        {
            handler(context, 0, "Failed to chain next async task");
        }
    }
    else
    {
        // Previous task failed, call the handler with failure
        if (handler)
        {
            handler(context, 0, error ? error : "Previous task in chain failed");
        }
    }
}
