#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ecewo.h"
#include "task.h"

// Thread pool work callback
static void task_work_cb(uv_work_t *req)
{
    Task *task = (Task *)req->data;
    if (task && task->work_fn)
        task->work_fn(task, task->context);
}

// Completion callback after thread work is done
static void task_completion_cb(uv_work_t *req, int status)
{
    Task *task = (Task *)req->data;
    if (!task)
        return;

    // Handle libuv errors
    if (status < 0)
    {
        // Try to get arena from task for error allocation
        if (task->arena)
        {
            task->error = arena_sprintf(task->arena,
                                        "libuv error: %s",
                                        uv_strerror(status));
        }
        else
        {
            task->error = "Task failed";
        }
    }

    // Call the result handler with result
    if (task->result_fn)
    {
        task->result_fn(task->context, task->error);
    }
}

// Creates and executes an async task
int task(
    Arena *arena,                 // Arena for memory allocation
    void *context,                // User context
    work_handler_t work_fn,       // Function to execute in the thread pool
    result_handler_t res_handler) // Result handler
{
    if (!arena || !context || !work_fn)
    {
        fprintf(stderr, "async task: arena, context and work_fn are required\n");
        return -1;
    }

    // Allocate task from provided arena
    Task *task = arena_alloc(arena, sizeof(Task));
    if (!task)
    {
        fprintf(stderr, "Failed to allocate memory for async task from arena\n");
        return -1;
    }

    // Initialize task
    task->work.data = task;
    task->context = context;
    task->arena = arena; // Store arena reference for error handling
    task->error = NULL;
    task->work_fn = work_fn;
    task->result_fn = res_handler;

    // Queue work in libuv thread pool
    int result = uv_queue_work(
        uv_default_loop(),
        &task->work,
        task_work_cb,
        task_completion_cb);

    if (result != 0)
    {
        fprintf(stderr, "Failed to queue task: %s\n", uv_strerror(result));
        return result;
    }

    return 0;
}
