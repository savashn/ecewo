#include "uv.h"
#include "ecewo.h"
#include <stdlib.h>

typedef struct
{
    uv_work_t work;
    uv_async_t async_send;
    void *context;
    spawn_handler_t work_fn;
    spawn_handler_t result_fn;
} spawn_t;

static void spawn_cleanup_cb(uv_handle_t *handle)
{
    spawn_t *t = (spawn_t *)handle->data;
    if (t)
        free(t);
}

static void spawn_async_cb(uv_async_t *handle)
{
    spawn_t *t = (spawn_t *)handle->data;
    if (!t)
        return;

    if (t->result_fn)
        t->result_fn(t->context);

    decrement_async_work();
    uv_close((uv_handle_t *)handle, spawn_cleanup_cb);
}

static void spawn_work_cb(uv_work_t *req)
{
    spawn_t *t = (spawn_t *)req->data;
    if (t && t->work_fn)
        t->work_fn(t->context);
}

static void spawn_after_work_cb(uv_work_t *req, int status)
{
    spawn_t *t = (spawn_t *)req->data;
    if (!t)
        return;

    if (status < 0)
        LOG_ERROR("Spawn execution failed");

    uv_async_send(&t->async_send);
}

int spawn(void *context, spawn_handler_t work_fn, spawn_handler_t done_fn)
{
    if (!context || !work_fn)
        return -1;

    spawn_t *task = calloc(1, sizeof(spawn_t));
    if (!task)
        return -1;

    if (uv_async_init(uv_default_loop(), &task->async_send, spawn_async_cb) != 0)
    {
        free(task);
        return -1;
    }

    task->work.data = task;
    task->async_send.data = task;
    task->context = context;
    task->work_fn = work_fn;
    task->result_fn = done_fn;

    increment_async_work();

    int result = uv_queue_work(
        uv_default_loop(),
        &task->work,
        spawn_work_cb,
        spawn_after_work_cb);

    if (result != 0)
    {
        uv_close((uv_handle_t *)&task->async_send, NULL);
        decrement_async_work();
        free(task);
        return result;
    }

    return 0;
}
