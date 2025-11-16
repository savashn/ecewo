#include "ecewo/static.h"
#include "ecewo/fs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

// ========================================================================
// MIME TYPE DETECTION
// ========================================================================

static const char *get_mime_type(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
        return "application/octet-stream";

    // Common web file types
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".js") == 0)
        return "application/javascript";
    if (strcmp(ext, ".json") == 0)
        return "application/json";
    if (strcmp(ext, ".xml") == 0)
        return "application/xml";

    // Images
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcmp(ext, ".svg") == 0)
        return "image/svg+xml";
    if (strcmp(ext, ".ico") == 0)
        return "image/x-icon";
    if (strcmp(ext, ".webp") == 0)
        return "image/webp";

    // Fonts
    if (strcmp(ext, ".woff") == 0)
        return "font/woff";
    if (strcmp(ext, ".woff2") == 0)
        return "font/woff2";
    if (strcmp(ext, ".ttf") == 0)
        return "font/ttf";
    if (strcmp(ext, ".otf") == 0)
        return "font/otf";

    // Documents
    if (strcmp(ext, ".pdf") == 0)
        return "application/pdf";
    if (strcmp(ext, ".txt") == 0)
        return "text/plain";

    // Media
    if (strcmp(ext, ".mp4") == 0)
        return "video/mp4";
    if (strcmp(ext, ".webm") == 0)
        return "video/webm";
    if (strcmp(ext, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(ext, ".wav") == 0)
        return "audio/wav";

    return "application/octet-stream";
}

// ========================================================================
// SECURITY - PATH TRAVERSAL PREVENTION
// ========================================================================

static bool is_safe_path(const char *path)
{
    if (strstr(path, "..") != NULL)
        return false;

    if (strstr(path, "//") != NULL)
        return false;

    return true;
}

// ========================================================================
// CONTEXT MANAGEMENT (Global Lookup Table)
// ========================================================================

typedef struct
{
    Res *res;
    const char *filepath;
    const char *mime_type;
} send_file_ctx_t;

#define MAX_CONTEXTS 100
static struct
{
    Res *key;
    send_file_ctx_t *value;
} g_context_table[MAX_CONTEXTS];
static int g_context_count = 0;

static void save_file_context(Res *res, send_file_ctx_t *ctx)
{
    if (g_context_count < MAX_CONTEXTS)
    {
        g_context_table[g_context_count].key = res;
        g_context_table[g_context_count].value = ctx;
        g_context_count++;
    }
}

static send_file_ctx_t *get_file_context(Res *res)
{
    for (int i = 0; i < g_context_count; i++)
    {
        if (g_context_table[i].key == res)
        {
            return g_context_table[i].value;
        }
    }
    return NULL;
}

static void remove_file_context(Res *res)
{
    for (int i = 0; i < g_context_count; i++)
    {
        if (g_context_table[i].key == res)
        {
            for (int j = i; j < g_context_count - 1; j++)
            {
                g_context_table[j] = g_context_table[j + 1];
            }
            g_context_count--;
            break;
        }
    }
}

// ========================================================================
// SEND FILE
// ========================================================================

static void on_file_read(FSRequest *fs_req, const char *error)
{
    Res *res = (Res *)fs_req->context;
    send_file_ctx_t *ctx = get_file_context(res);

    if (!ctx)
    {
        send_text(res, 500, "Internal error");
        return;
    }

    if (error)
    {
        remove_file_context(res);
        send_text(res, 404, "File not found");
        return;
    }

    reply(res, 200, ctx->mime_type, fs_req->data, fs_req->size);
    remove_file_context(res);
}

void send_file(Res *res, const char *filepath)
{
    if (!res || !filepath)
    {
        if (res)
            send_text(res, 500, "Invalid arguments");
        return;
    }

    if (!is_safe_path(filepath))
    {
        send_text(res, 403, "Forbidden");
        return;
    }

    send_file_ctx_t *ctx = ecewo_alloc(res, sizeof(send_file_ctx_t));
    if (!ctx)
    {
        send_text(res, 500, "Memory allocation failed");
        return;
    }

    ctx->res = res;
    ctx->filepath = filepath;
    ctx->mime_type = get_mime_type(filepath);

    save_file_context(res, ctx);

    fs_read_file(res, filepath, on_file_read);
}

// ========================================================================
// SERVE STATIC (Middleware)
// ========================================================================

typedef struct
{
    char *mount_path;
    char *dir_path;
    size_t mount_len;
    Static options;
} static_ctx_t;

typedef struct
{
    static_ctx_t **items;
    int count;
    int capacity;
} static_ctx_array_t;

static static_ctx_array_t static_contexts = {0};

static void ensure_static_capacity(void)
{
    if (static_contexts.count >= static_contexts.capacity)
    {
        int new_capacity = static_contexts.capacity == 0 ? 4 : static_contexts.capacity * 2;
        static_ctx_t **new_items = realloc(static_contexts.items,
                                           new_capacity * sizeof(static_ctx_t *));
        if (!new_items)
        {
            fprintf(stderr, "serve_static: Memory allocation failed\n");
            return;
        }
        static_contexts.items = new_items;
        static_contexts.capacity = new_capacity;
    }
}

static void static_handler(Req *req, Res *res)
{
    const char *url_path = req->path;

    for (int i = 0; i < static_contexts.count; i++)
    {
        static_ctx_t *ctx = static_contexts.items[i];

        if (strncmp(url_path, ctx->mount_path, ctx->mount_len) != 0)
            continue;

        const char *rel_path = url_path + ctx->mount_len;
        if (*rel_path == '/')
            rel_path++;

        if (!ctx->options.dot_files && *rel_path == '.')
        {
            send_text(res, 403, "Forbidden");
            return;
        }

        bool is_dir = (*rel_path == '\0' || 
                       (strlen(rel_path) > 0 && rel_path[strlen(rel_path) - 1] == '/'));

        char *filepath;
        if (is_dir)
        {
            if (strlen(rel_path) == 0)
            {
                filepath = ecewo_sprintf(res, "%s/%s", ctx->dir_path, ctx->options.index_file);
            }
            else
            {
                filepath = ecewo_sprintf(res, "%s/%s%s", ctx->dir_path, rel_path, ctx->options.index_file);
            }
        }
        else
        {
            filepath = ecewo_sprintf(res, "%s/%s", ctx->dir_path, rel_path);
        }

        if (!is_safe_path(filepath))
        {
            send_text(res, 403, "Forbidden");
            return;
        }

        send_file(res, filepath);
        return;
    }

    send_text(res, 404, "Not found");
}

void serve_static(const char *mount_path,
                  const char *dir_path,
                  const Static *options)
{
    if (!mount_path || !dir_path)
    {
        fprintf(stderr, "serve_static: Invalid arguments\n");
        return;
    }

    Static final_opts;
    
    final_opts.index_file = "index.html";
    final_opts.enable_etag = false;
    final_opts.enable_cache = false;
    final_opts.max_age = 3600;
    final_opts.dot_files = false;

    if (options)
    {
        if (options->index_file)
            final_opts.index_file = options->index_file;
        final_opts.enable_etag = options->enable_etag;
        final_opts.enable_cache = options->enable_cache;
        final_opts.max_age = options->max_age;
        final_opts.dot_files = options->dot_files;
    }

    ensure_static_capacity();
    if (static_contexts.count >= static_contexts.capacity)
    {
        fprintf(stderr, "serve_static: Failed to allocate memory\n");
        return;
    }

    static_ctx_t *ctx = malloc(sizeof(static_ctx_t));
    if (!ctx)
    {
        fprintf(stderr, "serve_static: Memory allocation failed\n");
        return;
    }

    ctx->mount_path = strdup(mount_path);
    ctx->dir_path = strdup(dir_path);
    ctx->mount_len = strlen(mount_path);
    ctx->options = final_opts;

    static_contexts.items[static_contexts.count++] = ctx;

    get(mount_path, static_handler);
    
    char *route_pattern = malloc(strlen(mount_path) + 4);
    if (mount_path[strlen(mount_path) - 1] == '/')
    {
        sprintf(route_pattern, "%s*", mount_path);
    }
    else
    {
        sprintf(route_pattern, "%s/*", mount_path);
    }

    get(route_pattern, static_handler);
    free(route_pattern);
}

void static_cleanup(void)
{
    for (int i = 0; i < static_contexts.count; i++)
    {
        static_ctx_t *ctx = static_contexts.items[i];
        if (ctx)
        {
            free(ctx->mount_path);
            free(ctx->dir_path);
            free(ctx);
        }
    }

    if (static_contexts.items)
    {
        free(static_contexts.items);
    }

    static_contexts.items = NULL;
    static_contexts.count = 0;
    static_contexts.capacity = 0;
}
