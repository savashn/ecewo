#ifndef ROUTE_TRIE_H
#define ROUTE_TRIE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "router.h"

#define MAX_PATH_SEGMENTS 128

// Path segment structure for pre-tokenized paths
typedef struct
{
    const char *start;
    size_t len;
    bool is_param;    // true if this segment is :param
    bool is_wildcard; // true if this segment is *
} path_segment_t;

// Tokenized path to avoid re-parsing
typedef struct
{
    path_segment_t *segments;
    uint8_t count;
    uint8_t capacity;
} tokenized_path_t;

typedef struct trie_node
{
    struct trie_node *children[128];  // ASCII characters
    struct trie_node *param_child;    // For :param segments
    struct trie_node *wildcard_child; // For * wildcard
    char *param_name;                 // Name of parameter if this is a param node
    bool is_end;                      // Marks end of a route
    RequestHandler handlers[8];       // Handlers for different HTTP methods
    void *middleware_ctx[8];          // Middleware context for each method
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

typedef enum
{
    METHOD_GET = 0,
    METHOD_POST = 1,
    METHOD_PUT = 2,
    METHOD_DELETE = 3,
    METHOD_PATCH = 4,
    METHOD_HEAD = 5,
    METHOD_OPTIONS = 6,
    METHOD_UNKNOWN = 7
} http_method_t;

// Path tokenization functions
int tokenize_path(Arena *arena, const char *path, tokenized_path_t *result);

// Now takes tokenized path instead of raw string
bool route_trie_match(route_trie_t *trie,
                      const char *method,
                      const tokenized_path_t *tokenized_path,
                      route_match_t *match);

// Existing functions remain same
route_trie_t *route_trie_create(void);
int route_trie_add(route_trie_t *trie, const char *method, const char *path,
                   RequestHandler handler, void *middleware_ctx);
void route_trie_free(route_trie_t *trie);
http_method_t get_method_index(const char *method);

#endif
