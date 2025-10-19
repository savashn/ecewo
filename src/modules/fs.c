#include "ecewo/fs.h"
#include <stdio.h>
#include <string.h>

// ========================================================================
// INTERNAL HELPERS
// ========================================================================

static char *make_error_msg(void *context, int errcode)
{
    Req *req = (Req *)context;
    return ecewo_sprintf(req, "%s: %s",
                         uv_err_name(errcode),
                         uv_strerror(errcode));
}

// ========================================================================
// READ FILE IMPLEMENTATION
// ========================================================================

typedef struct
{
    FSRequest *fs_req;
    uv_file file;
    size_t file_size;
} read_ctx_t;

static void read_close_cb(uv_fs_t *req)
{
    read_ctx_t *ctx = (read_ctx_t *)req->data;
    FSRequest *fs_req = ctx->fs_req;

    uv_fs_req_cleanup(req);
    fs_req->callback(fs_req, NULL);
}

static void read_data_cb(uv_fs_t *req)
{
    read_ctx_t *ctx = (read_ctx_t *)req->data;
    FSRequest *fs_req = ctx->fs_req;

    if (req->result < 0)
    {
        char *error = make_error_msg(fs_req->context, (int)req->result);
        uv_fs_req_cleanup(req);
        uv_fs_close(get_loop(), &fs_req->fs_req, ctx->file, NULL);
        fs_req->callback(fs_req, error);
        return;
    }

    fs_req->size = (size_t)req->result;
    fs_req->data[fs_req->size] = '\0';

    uv_fs_req_cleanup(req);

    fs_req->fs_req.data = ctx;
    uv_fs_close(get_loop(), &fs_req->fs_req, ctx->file, read_close_cb);
}

static void read_open_cb(uv_fs_t *req)
{
    read_ctx_t *ctx = (read_ctx_t *)req->data;
    FSRequest *fs_req = ctx->fs_req;
    Req *request = (Req *)fs_req->context;

    if (req->result < 0)
    {
        char *error = make_error_msg(fs_req->context, (int)req->result);
        uv_fs_req_cleanup(req);
        fs_req->callback(fs_req, error);
        return;
    }

    ctx->file = (uv_file)req->result;
    uv_fs_req_cleanup(req);

    fs_req->data = ecewo_alloc(request, ctx->file_size + 1);
    if (!fs_req->data)
    {
        uv_fs_close(get_loop(), &fs_req->fs_req, ctx->file, NULL);
        fs_req->callback(fs_req, "Memory allocation failed");
        return;
    }

    uv_buf_t buf = uv_buf_init(fs_req->data, (unsigned int)ctx->file_size);
    fs_req->fs_req.data = ctx;
    uv_fs_read(get_loop(), &fs_req->fs_req, ctx->file, &buf, 1, 0, read_data_cb);
}

static void read_stat_cb(uv_fs_t *req)
{
    read_ctx_t *ctx = (read_ctx_t *)req->data;
    FSRequest *fs_req = ctx->fs_req;

    if (req->result < 0)
    {
        char *error = make_error_msg(fs_req->context, (int)req->result);
        uv_fs_req_cleanup(req);
        fs_req->callback(fs_req, error);
        return;
    }

    ctx->file_size = (size_t)req->statbuf.st_size;
    uv_fs_req_cleanup(req);

    fs_req->fs_req.data = ctx;
    uv_fs_open(get_loop(), &fs_req->fs_req, (const char *)fs_req->data,
               UV_FS_O_RDONLY, 0, read_open_cb);
}

void fs_read_file(void *context, const char *path, fs_callback_t callback)
{
    FSRequest *fs_req;
    read_ctx_t *ctx;
    const char *path_copy;
    int result;

    if (!context || !path || !callback)
    {
        fprintf(stderr, "fs_read_file: Invalid arguments\n");
        return;
    }

    Req *req = (Req *)context;

    fs_req = ecewo_alloc(req, sizeof(FSRequest));
    if (!fs_req)
    {
        fprintf(stderr, "fs_read_file: Memory allocation failed\n");
        return;
    }

    memset(fs_req, 0, sizeof(FSRequest));
    fs_req->context = context;
    fs_req->callback = callback;

    ctx = ecewo_alloc(req, sizeof(read_ctx_t));
    if (!ctx)
    {
        callback(fs_req, "Memory allocation failed");
        return;
    }

    memset(ctx, 0, sizeof(read_ctx_t));
    ctx->fs_req = fs_req;

    path_copy = ecewo_strdup(req, path);
    fs_req->data = (char *)path_copy;

    fs_req->fs_req.data = ctx;
    result = uv_fs_stat(get_loop(), &fs_req->fs_req, path_copy, read_stat_cb);

    if (result < 0)
    {
        char *error = make_error_msg(context, result);
        callback(fs_req, error);
    }
}

// ========================================================================
// WRITE FILE IMPLEMENTATION
// ========================================================================

typedef struct
{
    FSRequest *fs_req;
    uv_file file;
    uv_buf_t buf;
} write_ctx_t;

static void write_close_cb(uv_fs_t *req)
{
    write_ctx_t *ctx = (write_ctx_t *)req->data;
    FSRequest *fs_req = ctx->fs_req;

    uv_fs_req_cleanup(req);
    fs_req->callback(fs_req, NULL);
}

static void write_data_cb(uv_fs_t *req)
{
    write_ctx_t *ctx = (write_ctx_t *)req->data;
    FSRequest *fs_req = ctx->fs_req;

    if (req->result < 0)
    {
        char *error = make_error_msg(fs_req->context, (int)req->result);
        uv_fs_req_cleanup(req);
        uv_fs_close(get_loop(), &fs_req->fs_req, ctx->file, NULL);
        fs_req->callback(fs_req, error);
        return;
    }

    fs_req->size = (size_t)req->result;
    uv_fs_req_cleanup(req);

    fs_req->fs_req.data = ctx;
    uv_fs_close(get_loop(), &fs_req->fs_req, ctx->file, write_close_cb);
}

static void write_open_cb(uv_fs_t *req)
{
    write_ctx_t *ctx = (write_ctx_t *)req->data;
    FSRequest *fs_req = ctx->fs_req;

    if (req->result < 0)
    {
        char *error = make_error_msg(fs_req->context, (int)req->result);
        uv_fs_req_cleanup(req);
        fs_req->callback(fs_req, error);
        return;
    }

    ctx->file = (uv_file)req->result;
    uv_fs_req_cleanup(req);

    fs_req->fs_req.data = ctx;
    uv_fs_write(get_loop(), &fs_req->fs_req, ctx->file, &ctx->buf, 1, 0, write_data_cb);
}

void fs_write_file(void *context, const char *path,
                   const void *data, size_t size, fs_callback_t callback)
{
    FSRequest *fs_req;
    write_ctx_t *ctx;
    void *data_copy;
    const char *path_copy;
    int result;

    if (!context || !path || !data || !callback)
    {
        fprintf(stderr, "fs_write_file: Invalid arguments\n");
        return;
    }

    Req *req = (Req *)context;

    fs_req = ecewo_alloc(req, sizeof(FSRequest));
    if (!fs_req)
    {
        fprintf(stderr, "fs_write_file: Memory allocation failed\n");
        return;
    }

    memset(fs_req, 0, sizeof(FSRequest));
    fs_req->context = context;
    fs_req->callback = callback;

    ctx = ecewo_alloc(req, sizeof(write_ctx_t));
    if (!ctx)
    {
        callback(fs_req, "Memory allocation failed");
        return;
    }

    memset(ctx, 0, sizeof(write_ctx_t));
    ctx->fs_req = fs_req;

    data_copy = ecewo_memdup(req, (void *)data, size);
    if (!data_copy)
    {
        callback(fs_req, "Memory allocation failed");
        return;
    }

    ctx->buf = uv_buf_init(data_copy, (unsigned int)size);
    path_copy = ecewo_strdup(req, path);

    fs_req->fs_req.data = ctx;
    result = uv_fs_open(get_loop(), &fs_req->fs_req, path_copy,
                        UV_FS_O_WRONLY | UV_FS_O_CREAT | UV_FS_O_TRUNC,
                        0644, write_open_cb);

    if (result < 0)
    {
        char *error = make_error_msg(context, result);
        callback(fs_req, error);
    }
}

void fs_append_file(void *context, const char *path,
                    const void *data, size_t size, fs_callback_t callback)
{
    FSRequest *fs_req;
    write_ctx_t *ctx;
    void *data_copy;
    const char *path_copy;
    int result;

    if (!context || !path || !data || !callback)
    {
        fprintf(stderr, "fs_append_file: Invalid arguments\n");
        return;
    }

    Req *req = (Req *)context;

    fs_req = ecewo_alloc(req, sizeof(FSRequest));
    ctx = ecewo_alloc(req, sizeof(write_ctx_t));

    if (!fs_req || !ctx)
    {
        if (fs_req)
            callback(fs_req, "Memory allocation failed");
        return;
    }

    memset(fs_req, 0, sizeof(FSRequest));
    memset(ctx, 0, sizeof(write_ctx_t));

    fs_req->context = context;
    fs_req->callback = callback;
    ctx->fs_req = fs_req;

    data_copy = ecewo_memdup(req, (void *)data, size);
    if (!data_copy)
    {
        callback(fs_req, "Memory allocation failed");
        return;
    }

    ctx->buf = uv_buf_init(data_copy, (unsigned int)size);
    path_copy = ecewo_strdup(req, path);

    fs_req->fs_req.data = ctx;
    result = uv_fs_open(get_loop(), &fs_req->fs_req, path_copy,
                        UV_FS_O_WRONLY | UV_FS_O_CREAT | UV_FS_O_APPEND,
                        0644, write_open_cb);

    if (result < 0)
    {
        char *error = make_error_msg(context, result);
        callback(fs_req, error);
    }
}

// ========================================================================
// SIMPLE OPERATIONS
// ========================================================================

static void simple_op_cb(uv_fs_t *req)
{
    FSRequest *fs_req = (FSRequest *)req->data;
    char *error = NULL;

    if (req->result < 0)
    {
        error = make_error_msg(fs_req->context, (int)req->result);
    }
    else if (req->fs_type == UV_FS_STAT)
    {
        fs_req->stat = req->statbuf;
    }

    uv_fs_req_cleanup(req);
    fs_req->callback(fs_req, error);
}

void fs_stat(void *context, const char *path, fs_callback_t callback)
{
    FSRequest *fs_req;
    const char *path_copy;
    int result;

    if (!context || !path || !callback)
        return;

    Req *req = (Req *)context;
    fs_req = ecewo_alloc(req, sizeof(FSRequest));
    if (!fs_req)
        return;

    memset(fs_req, 0, sizeof(FSRequest));
    fs_req->context = context;
    fs_req->callback = callback;
    fs_req->fs_req.data = fs_req;

    path_copy = ecewo_strdup(req, path);
    result = uv_fs_stat(get_loop(), &fs_req->fs_req, path_copy, simple_op_cb);

    if (result < 0)
    {
        char *error = make_error_msg(context, result);
        callback(fs_req, error);
    }
}

void fs_unlink(void *context, const char *path, fs_callback_t callback)
{
    FSRequest *fs_req;
    const char *path_copy;
    int result;

    if (!context || !path || !callback)
        return;

    Req *req = (Req *)context;
    fs_req = ecewo_alloc(req, sizeof(FSRequest));
    if (!fs_req)
        return;

    memset(fs_req, 0, sizeof(FSRequest));
    fs_req->context = context;
    fs_req->callback = callback;
    fs_req->fs_req.data = fs_req;

    path_copy = ecewo_strdup(req, path);
    result = uv_fs_unlink(get_loop(), &fs_req->fs_req, path_copy, simple_op_cb);

    if (result < 0)
    {
        char *error = make_error_msg(context, result);
        callback(fs_req, error);
    }
}

void fs_rename(void *context, const char *old_path, const char *new_path,
               fs_callback_t callback)
{
    FSRequest *fs_req;
    const char *old_copy;
    const char *new_copy;
    int result;

    if (!context || !old_path || !new_path || !callback)
        return;

    Req *req = (Req *)context;
    fs_req = ecewo_alloc(req, sizeof(FSRequest));
    if (!fs_req)
        return;

    memset(fs_req, 0, sizeof(FSRequest));
    fs_req->context = context;
    fs_req->callback = callback;
    fs_req->fs_req.data = fs_req;

    old_copy = ecewo_strdup(req, old_path);
    new_copy = ecewo_strdup(req, new_path);

    result = uv_fs_rename(get_loop(), &fs_req->fs_req, old_copy, new_copy, simple_op_cb);

    if (result < 0)
    {
        char *error = make_error_msg(context, result);
        callback(fs_req, error);
    }
}

void fs_mkdir(void *context, const char *path, fs_callback_t callback)
{
    FSRequest *fs_req;
    const char *path_copy;
    int result;

    if (!context || !path || !callback)
        return;

    Req *req = (Req *)context;
    fs_req = ecewo_alloc(req, sizeof(FSRequest));
    if (!fs_req)
        return;

    memset(fs_req, 0, sizeof(FSRequest));
    fs_req->context = context;
    fs_req->callback = callback;
    fs_req->fs_req.data = fs_req;

    path_copy = ecewo_strdup(req, path);
    result = uv_fs_mkdir(get_loop(), &fs_req->fs_req, path_copy, 0755, simple_op_cb);

    if (result < 0)
    {
        char *error = make_error_msg(context, result);
        callback(fs_req, error);
    }
}

void fs_rmdir(void *context, const char *path, fs_callback_t callback)
{
    FSRequest *fs_req;
    const char *path_copy;
    int result;

    if (!context || !path || !callback)
        return;

    Req *req = (Req *)context;
    fs_req = ecewo_alloc(req, sizeof(FSRequest));
    if (!fs_req)
        return;

    memset(fs_req, 0, sizeof(FSRequest));
    fs_req->context = context;
    fs_req->callback = callback;
    fs_req->fs_req.data = fs_req;

    path_copy = ecewo_strdup(req, path);
    result = uv_fs_rmdir(get_loop(), &fs_req->fs_req, path_copy, simple_op_cb);

    if (result < 0)
    {
        char *error = make_error_msg(context, result);
        callback(fs_req, error);
    }
}
