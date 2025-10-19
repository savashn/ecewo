#ifndef ECEWO_FS_H
#define ECEWO_FS_H

#include <stddef.h>
#include "ecewo.h"
#include "uv.h"

#include "ecewo-config.h"

#ifndef ECEWO_HAS_FS
#error "File serving module is not enabled. Build with -DECEWO_FS=ON"
#endif

typedef struct Req Req;
typedef struct Res Res;
typedef struct FSRequest FSRequest;

typedef void (*fs_callback_t)(FSRequest *req, const char *error);

struct FSRequest
{
    uv_fs_t fs_req;
    void *context;
    fs_callback_t callback;

    // Result data
    char *data;
    size_t size;
    uv_stat_t stat;
};

void fs_read_file(void *context, const char *path, fs_callback_t callback);

void fs_write_file(void *context, const char *path,
                   const void *data, size_t size, fs_callback_t callback);

void fs_append_file(void *context, const char *path,
                    const void *data, size_t size, fs_callback_t callback);

// Get file stats - result in req->stat
void fs_stat(void *context, const char *path, fs_callback_t callback);

void fs_unlink(void *context, const char *path, fs_callback_t callback);

void fs_rename(void *context, const char *old_path, const char *new_path,
               fs_callback_t callback);

void fs_mkdir(void *context, const char *path, fs_callback_t callback);

void fs_rmdir(void *context, const char *path, fs_callback_t callback);

#endif
