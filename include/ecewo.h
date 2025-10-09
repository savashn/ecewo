#ifndef ECEWO_H
#define ECEWO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

typedef struct uv_loop_s uv_loop_t;
typedef struct uv_timer_s uv_timer_t;
typedef struct uv_tcp_s uv_tcp_t;

// ============================================================================
// FORWARD DECLARATIONS - Arena
// ============================================================================

typedef struct Arena Arena;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

typedef struct Req Req;
typedef struct Res Res;
typedef struct Chain Chain;

// ============================================================================
// PUBLIC TYPES
// ============================================================================

// HTTP Status Codes
typedef enum
{
    // 1xx Informational
    CONTINUE = 100,
    SWITCHING_PROTOCOLS = 101,
    PROCESSING = 102,
    EARLY_HINTS = 103,

    // 2xx Success
    OK = 200,
    CREATED = 201,
    ACCEPTED = 202,
    NON_AUTHORITATIVE_INFORMATION = 203,
    NO_CONTENT = 204,
    RESET_CONTENT = 205,
    PARTIAL_CONTENT = 206,
    MULTI_STATUS = 207,
    ALREADY_REPORTED = 208,
    IM_USED = 226,

    // 3xx Redirection
    MULTIPLE_CHOICES = 300,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    SEE_OTHER = 303,
    NOT_MODIFIED = 304,
    USE_PROXY = 305,
    TEMPORARY_REDIRECT = 307,
    PERMANENT_REDIRECT = 308,

    // 4xx Client Error
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    PAYMENT_REQUIRED = 402,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    NOT_ACCEPTABLE = 406,
    PROXY_AUTHENTICATION_REQUIRED = 407,
    REQUEST_TIMEOUT = 408,
    CONFLICT = 409,
    GONE = 410,
    LENGTH_REQUIRED = 411,
    PRECONDITION_FAILED = 412,
    PAYLOAD_TOO_LARGE = 413,
    URI_TOO_LONG = 414,
    UNSUPPORTED_MEDIA_TYPE = 415,
    RANGE_NOT_SATISFIABLE = 416,
    EXPECTATION_FAILED = 417,
    IM_A_TEAPOT = 418,
    MISDIRECTED_REQUEST = 421,
    UNPROCESSABLE_ENTITY = 422,
    LOCKED = 423,
    FAILED_DEPENDENCY = 424,
    TOO_EARLY = 425,
    UPGRADE_REQUIRED = 426,
    PRECONDITION_REQUIRED = 428,
    TOO_MANY_REQUESTS = 429,
    REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
    UNAVAILABLE_FOR_LEGAL_REASONS = 451,

    // 5xx Server Error
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
    GATEWAY_TIMEOUT = 504,
    HTTP_VERSION_NOT_SUPPORTED = 505,
    VARIANT_ALSO_NEGOTIATES = 506,
    INSUFFICIENT_STORAGE = 507,
    LOOP_DETECTED = 508,
    NOT_EXTENDED = 510,
    NETWORK_AUTHENTICATION_REQUIRED = 511
} http_status_t;

typedef void (*RequestHandler)(Req *req, Res *res);

// ============================================================================
// PUBLIC TYPES
// ============================================================================

typedef int (*MiddlewareHandler)(Req *req, Res *res, Chain *chain);

typedef struct
{
    MiddlewareHandler *handlers;
    size_t count;
} MiddlewareArray;

#define use(...)                                        \
    ((MiddlewareArray){                                 \
        .handlers = (MiddlewareHandler[]){__VA_ARGS__}, \
        .count = sizeof((MiddlewareHandler[]){__VA_ARGS__}) / sizeof(MiddlewareHandler)})

#define NO_MW ((MiddlewareArray){.handlers = NULL, .count = 0})

// ============================================================================
// PUBLIC TYPES
// ============================================================================

typedef enum
{
    SERVER_OK = 0,
    SERVER_ALREADY_INITIALIZED = -1,
    SERVER_NOT_INITIALIZED = -2,
    SERVER_ALREADY_RUNNING = -3,
    SERVER_INIT_FAILED = -4,
    SERVER_OUT_OF_MEMORY = -5,
    SERVER_BIND_FAILED = -6,
    SERVER_LISTEN_FAILED = -7,
    SERVER_INVALID_PORT = -8,
} server_error_t;

typedef void (*shutdown_callback_t)(void);
typedef void (*timer_callback_t)(void *user_data);

// ============================================================================
// PUBLIC TYPES
// ============================================================================

typedef struct task_s Task;
typedef void (*result_handler_t)(void *context, char *error);
typedef void (*work_handler_t)(Task *task, void *context);

// ============================================================================
// Server Functions
// ============================================================================

int server_init(void);
int server_listen(uint16_t port);
void server_run(void);
void shutdown_hook(shutdown_callback_t callback);
bool server_is_running(void);
int get_active_connections(void);
uv_loop_t *get_loop(void);

// ============================================================================
// Timer Functions
// ============================================================================

uv_timer_t *set_timeout(timer_callback_t callback, uint64_t delay_ms, void *user_data);
uv_timer_t *set_interval(timer_callback_t callback, uint64_t interval_ms, void *user_data);
void clear_timer(uv_timer_t *timer);

// ============================================================================
// Request Functions
// ============================================================================

const char *get_param(const Req *req, const char *key);
const char *get_query(const Req *req, const char *key);
const char *get_header(const Req *req, const char *key);
void set_context(Req *req, const char *key, void *data, size_t size);
void *get_context(Req *req, const char *key);

// ============================================================================
// Response Functions
// ============================================================================

void set_header(Res *res, const char *name, const char *value);
void reply(Res *res, int status, const char *content_type, const void *body, size_t body_len);
void redirect(Res *res, int status, const char *url);

static inline void send_text(Res *res, int status, const char *body)
{
    reply(res, status, "text/plain", body, strlen(body));
}

static inline void send_html(Res *res, int status, const char *body)
{
    reply(res, status, "text/html", body, strlen(body));
}

static inline void send_json(Res *res, int status, const char *body)
{
    reply(res, status, "application/json", body, strlen(body));
}

static inline void send_cbor(Res *res, int status, const char *body, size_t body_len)
{
    reply(res, status, "application/cbor", body, body_len);
}

// ============================================================================
// Memory Functions (Arena)
// ============================================================================

void *arena_alloc(Arena *a, size_t size_bytes);
void *arena_realloc(Arena *a, void *oldptr, size_t oldsz, size_t newsz);
char *arena_strdup(Arena *a, const char *cstr);
void *arena_memdup(Arena *a, void *data, size_t size);
char *arena_sprintf(Arena *a, const char *format, ...);
void *arena_memcpy(void *dest, const void *src, size_t n);

#define ecewo_alloc(x, size_bytes) \
    arena_alloc((x)->arena, size_bytes)

#define ecewo_realloc(x, oldptr, oldsz, newsz) \
    arena_realloc((x)->arena, oldptr, oldsz, newsz)

#define ecewo_strdup(x, cstr) \
    arena_strdup((x)->arena, cstr)

#define ecewo_memdup(x, data, size) \
    arena_memdup((x)->arena, data, size)

#define ecewo_sprintf(x, format, ...) \
    arena_sprintf((x)->arena, format, ##__VA_ARGS__)

// ============================================================================
// Middleware Functions
// ============================================================================

void hook(MiddlewareHandler middleware_handler);
int next(Req *req, Res *res, Chain *chain);

// ============================================================================
// Task Functions
// ============================================================================

int task(Arena *arena, void *context, work_handler_t work_fn, result_handler_t result_fn);

#define worker(context, work_fn, result_fn) \
    task((context)->res->arena, (context), (work_fn), (result_fn))

// ============================================================================
// Route Registration
// ============================================================================

// Internal functions (used by macros below)
void register_sync_route(int method, const char *path, MiddlewareArray middleware, RequestHandler handler);
void register_async_route(int method, const char *path, MiddlewareArray middleware, RequestHandler handler);

// ============================================================================
// Synchronous Route Macros
// ============================================================================

#define GET_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define get(...) \
    GET_CHOOSER(__VA_ARGS__, get_with_mw, get_no_mw)(__VA_ARGS__)

static inline void get_no_mw(const char *path, RequestHandler handler)
{
    register_sync_route(HTTP_GET, path, NO_MW, handler);
}

static inline void get_with_mw(const char *path, MiddlewareArray mw, RequestHandler handler)
{
    register_sync_route(HTTP_GET, path, mw, handler);
}

#define POST_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define post(...) \
    POST_CHOOSER(__VA_ARGS__, post_with_mw, post_no_mw)(__VA_ARGS__)

static inline void post_no_mw(const char *p, RequestHandler h)
{
    register_sync_route(HTTP_POST, p, NO_MW, h);
}

static inline void post_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_sync_route(HTTP_POST, p, mw, h);
}

#define PUT_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define put(...) \
    PUT_CHOOSER(__VA_ARGS__, put_with_mw, put_no_mw)(__VA_ARGS__)

static inline void put_no_mw(const char *p, RequestHandler h)
{
    register_sync_route(HTTP_PUT, p, NO_MW, h);
}

static inline void put_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_sync_route(HTTP_PUT, p, mw, h);
}

#define PATCH_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define patch(...) \
    PATCH_CHOOSER(__VA_ARGS__, patch_with_mw, patch_no_mw)(__VA_ARGS__)

static inline void patch_no_mw(const char *p, RequestHandler h)
{
    register_sync_route(HTTP_PATCH, p, NO_MW, h);
}

static inline void patch_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_sync_route(HTTP_PATCH, p, mw, h);
}

#define DEL_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define del(...) \
    DEL_CHOOSER(__VA_ARGS__, del_with_mw, del_no_mw)(__VA_ARGS__)

static inline void del_no_mw(const char *p, RequestHandler h)
{
    register_sync_route(HTTP_DELETE, p, NO_MW, h);
}

static inline void del_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_sync_route(HTTP_DELETE, p, mw, h);
}

// ============================================================================
// Asynchronous Route Macros
// ============================================================================

#define GET_ASYNC_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define get_worker(...) \
    GET_ASYNC_CHOOSER(__VA_ARGS__, get_async_with_mw, get_async_no_mw)(__VA_ARGS__)

static inline void get_async_no_mw(const char *path, RequestHandler handler)
{
    register_async_route(HTTP_GET, path, NO_MW, handler);
}

static inline void get_async_with_mw(const char *path, MiddlewareArray mw, RequestHandler handler)
{
    register_async_route(HTTP_GET, path, mw, handler);
}

#define POST_ASYNC_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define post_worker(...) \
    POST_ASYNC_CHOOSER(__VA_ARGS__, post_async_with_mw, post_async_no_mw)(__VA_ARGS__)

static inline void post_async_no_mw(const char *p, RequestHandler h)
{
    register_async_route(HTTP_POST, p, NO_MW, h);
}

static inline void post_async_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_async_route(HTTP_POST, p, mw, h);
}

#define PUT_ASYNC_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define put_worker(...) \
    PUT_ASYNC_CHOOSER(__VA_ARGS__, put_async_with_mw, put_async_no_mw)(__VA_ARGS__)

static inline void put_async_no_mw(const char *p, RequestHandler h)
{
    register_async_route(HTTP_PUT, p, NO_MW, h);
}

static inline void put_async_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_async_route(HTTP_PUT, p, mw, h);
}

#define PATCH_ASYNC_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define patch_worker(...) \
    PATCH_ASYNC_CHOOSER(__VA_ARGS__, patch_async_with_mw, patch_async_no_mw)(__VA_ARGS__)

static inline void patch_async_no_mw(const char *p, RequestHandler h)
{
    register_async_route(HTTP_PATCH, p, NO_MW, h);
}

static inline void patch_async_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_async_route(HTTP_PATCH, p, mw, h);
}

#define DEL_ASYNC_CHOOSER(_1, _2, _3, NAME, ...) NAME
#define del_worker(...) \
    DEL_ASYNC_CHOOSER(__VA_ARGS__, del_async_with_mw, del_async_no_mw)(__VA_ARGS__)

static inline void del_async_no_mw(const char *p, RequestHandler h)
{
    register_async_route(HTTP_DELETE, p, NO_MW, h);
}

static inline void del_async_with_mw(const char *p, MiddlewareArray mw, RequestHandler h)
{
    register_async_route(HTTP_DELETE, p, mw, h);
}

#endif
