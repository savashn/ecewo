#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "task.h"

static void task_work_cb(uv_work_t *req)
{
    Task *task = (Task *)req->data;
    if (task && task->work_fn)
        task->work_fn(task, task->context);
}

static void task_completion_cb(uv_work_t *req, int status)
{
    Task *task = (Task *)req->data;
    if (!task)
        return;

    if (status < 0)
    {
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

    if (task->result_fn)
    {
        task->result_fn(task->context, task->error);
    }

    decrement_async_work();
}

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

    Task *task = arena_alloc(arena, sizeof(Task));
    if (!task)
    {
        fprintf(stderr, "Failed to allocate memory for async task from arena\n");
        return -1;
    }

    task->work.data = task;
    task->context = context;
    task->arena = arena; // Store arena reference for error handling
    task->error = NULL;
    task->work_fn = work_fn;
    task->result_fn = res_handler;

    increment_async_work();

    int result = uv_queue_work(
        uv_default_loop(),
        &task->work,
        task_work_cb,
        task_completion_cb);

    if (result != 0)
    {
        fprintf(stderr, "Failed to queue task: %s\n", uv_strerror(result));
        decrement_async_work();
        return result;
    }

    return 0;
}
