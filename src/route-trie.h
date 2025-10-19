#ifndef ROUTE_TRIE_H
#define ROUTE_TRIE_H

#include "ecewo.h"
#include "router.h"
#include "llhttp.h"
#include "uv.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_PATH_SEGMENTS 128

typedef enum
{
    METHOD_INDEX_DELETE = 0,
    METHOD_INDEX_GET = 1,
    METHOD_INDEX_HEAD = 2,
    METHOD_INDEX_POST = 3,
    METHOD_INDEX_PUT = 4,
    METHOD_INDEX_OPTIONS = 5,
    METHOD_INDEX_PATCH = 6,
    METHOD_COUNT = 7
} http_method_index_t;

typedef struct
{
    const char *start;
    size_t len;
    bool is_param;
    bool is_wildcard;
} path_segment_t;

typedef struct
{
    path_segment_t *segments;
    uint8_t count;
    uint8_t capacity;
} tokenized_path_t;

typedef struct trie_node
{
    struct trie_node *children[128];       // ASCII characters
    struct trie_node *param_child;         // For :param segments
    struct trie_node *wildcard_child;      // For * wildcard
    char *param_name;                      // Name of parameter if this is a param node
    bool is_end;                           // Marks end of a route
    RequestHandler handlers[METHOD_COUNT]; // Handlers for different HTTP methods
    void *middleware_ctx[METHOD_COUNT];    // Middleware context for each method
} trie_node_t;

typedef struct
{
    trie_node_t *root;
    size_t route_count;
    uv_rwlock_t lock;
} route_trie_t;

extern route_trie_t *global_route_trie;

typedef struct
{
    const char *data;
    size_t len;
} string_view_t;

typedef struct
{
    string_view_t key;
    string_view_t value;
} param_match_t;

typedef struct
{
    RequestHandler handler;
    void *middleware_ctx;
    param_match_t params[32];
    uint8_t param_count;
} route_match_t;

// Convert llhttp_method_t to internal index
// Returns -1 for unsupported methods
static inline int method_to_index(llhttp_method_t method)
{
    switch (method)
    {
    case HTTP_DELETE:
        return METHOD_INDEX_DELETE;
    case HTTP_GET:
        return METHOD_INDEX_GET;
    case HTTP_HEAD:
        return METHOD_INDEX_HEAD;
    case HTTP_POST:
        return METHOD_INDEX_POST;
    case HTTP_PUT:
        return METHOD_INDEX_PUT;
    case HTTP_OPTIONS:
        return METHOD_INDEX_OPTIONS;
    case HTTP_PATCH:
        return METHOD_INDEX_PATCH;
    default:
        return -1;
    }
}

int tokenize_path(Arena *arena, const char *path, tokenized_path_t *result);
bool route_trie_match(route_trie_t *trie,
                      llhttp_t *parser,
                      const tokenized_path_t *tokenized_path,
                      route_match_t *match);
route_trie_t *route_trie_create(void);
int route_trie_add(route_trie_t *trie,
                   llhttp_method_t method,
                   const char *path,
                   RequestHandler handler,
                   void *middleware_ctx);
void route_trie_free(route_trie_t *trie);

#endif
