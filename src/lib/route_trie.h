#ifndef ROUTE_TRIE_H
#define ROUTE_TRIE_H

#include <stddef.h>
#include <stdbool.h>
#include "router.h"

// Trie node for efficient route matching
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

// HTTP method enum for indexing
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

// Route trie structure
typedef struct
{
    trie_node_t *root;
    size_t route_count;
    uv_rwlock_t lock; // Read-write lock for thread safety
} route_trie_t;

// String view for zero-copy operations
typedef struct
{
    const char *data;
    size_t len;
} string_view_t;

// Match result with extracted parameters
typedef struct
{
    RequestHandler handler;
    void *middleware_ctx;
    struct
    {
        string_view_t key;
        string_view_t value;
    } params[32]; // Max 32 parameters
    int param_count;
} route_match_t;

// Initialize route trie
route_trie_t *route_trie_create(void);

// Add a route to the trie
int route_trie_add(route_trie_t *trie, const char *method, const char *path,
                   RequestHandler handler, void *middleware_ctx);

// Find a matching route
bool route_trie_match(route_trie_t *trie, const char *method, const char *path,
                      route_match_t *match);

// Free the trie
void route_trie_free(route_trie_t *trie);

// Get method index from string
http_method_t get_method_index(const char *method);

#endif
