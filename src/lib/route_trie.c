#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "route_trie.h"
#include "middleware.h"

// Splits a path into segments (/users/123/posts -> ["users", "123", "posts"])
int tokenize_path(Arena *arena, const char *path, tokenized_path_t *result)
{
    if (!path || !result)
        return -1;

    // Initialize result
    memset(result, 0, sizeof(tokenized_path_t));

    // Skip leading slash
    if (*path == '/')
        path++;

    // Handle root path
    if (*path == '\0')
        return 0;

    // Count segments first
    int segment_count = 0;
    const char *p = path;
    while (*p)
    {
        if (*p != '/')
        {
            segment_count++;
            // Skip to next '/' or end
            while (*p && *p != '/')
                p++;
        }
        else
        {
            p++;
        }
    }

    if (segment_count == 0)
        return 0;

    // Allocate segments
    result->capacity = segment_count;
    result->segments = arena_alloc(arena, sizeof(path_segment_t) * segment_count);
    if (!result->segments)
        return -1;

    // Parse segments
    p = path;
    result->count = 0;

    while (*p && result->count < result->capacity)
    {
        // Skip slashes
        while (*p == '/')
            p++;
        if (!*p)
            break;

        const char *start = p;

        // Find end of segment
        while (*p && *p != '/')
            p++;

        size_t len = p - start;
        if (len == 0)
            continue;

        // Analyze segment type
        path_segment_t *seg = &result->segments[result->count];
        seg->start = start;
        seg->len = len;
        seg->is_param = (start[0] == ':');
        seg->is_wildcard = (start[0] == '*');

        result->count++;
    }

    return 0;
}

static trie_node_t *match_segments(trie_node_t *node,
                                   const tokenized_path_t *path,
                                   int segment_idx,
                                   route_match_t *match,
                                   int depth)
{
    if (!node || depth > 100)
        return NULL;

    // All segments processed
    if (segment_idx >= path->count)
    {
        return node->is_end ? node : NULL;
    }

    const path_segment_t *segment = &path->segments[segment_idx];

    // Try exact match first (only for non-param segments)
    if (!segment->is_param && !segment->is_wildcard)
    {
        trie_node_t *current = node;

        // Match character by character
        for (size_t i = 0; i < segment->len && current; i++)
        {
            unsigned char c = (unsigned char)segment->start[i];
            current = current->children[c];
        }

        // If exact match succeeded
        if (current)
        {
            if (segment_idx + 1 >= path->count)
            {
                // Last segment
                if (current->is_end)
                    return current;
            }
            else
            {
                // More segments - need slash separator
                unsigned char sep = '/';
                if (current->children[sep])
                {
                    trie_node_t *result = match_segments(
                        current->children[sep], path, segment_idx + 1, match, depth + 1);
                    if (result)
                        return result;
                }
            }
        }
    }

    // Try parameter match
    if (node->param_child)
    {
        // Store parameter value
        if (match && match->param_count < 32)
        {
            match->params[match->param_count].key.data = node->param_child->param_name;
            match->params[match->param_count].key.len = strlen(node->param_child->param_name);
            match->params[match->param_count].value.data = segment->start;
            match->params[match->param_count].value.len = segment->len;
            match->param_count++;
        }

        // Continue matching
        if (segment_idx + 1 >= path->count)
        {
            if (node->param_child->is_end)
                return node->param_child;
        }
        else
        {
            unsigned char sep = '/';
            if (node->param_child->children[sep])
            {
                trie_node_t *result = match_segments(
                    node->param_child->children[sep], path, segment_idx + 1, match, depth + 1);
                if (result)
                    return result;
            }
        }

        // Backtrack parameter
        if (match && match->param_count > 0)
        {
            match->param_count--;
        }
    }

    // Try wildcard match
    if (node->wildcard_child && node->wildcard_child->is_end)
    {
        return node->wildcard_child;
    }

    return NULL;
}

// Create a new trie node
static trie_node_t *trie_node_create(void)
{
    trie_node_t *node = calloc(1, sizeof(trie_node_t));
    if (!node)
        return NULL;

    node->is_end = false;
    return node;
}

// Free a trie node and its children recursively
static void trie_node_free(trie_node_t *node)
{
    if (!node)
        return;

    // Free all children
    for (int i = 0; i < 128; i++)
    {
        if (node->children[i])
        {
            trie_node_free(node->children[i]);
        }
    }

    // Free param and wildcard children
    if (node->param_child)
    {
        trie_node_free(node->param_child);
    }

    if (node->wildcard_child)
    {
        trie_node_free(node->wildcard_child);
    }

    // Free middleware contexts
    if (node->is_end)
    {
        for (int i = 0; i < 8; i++)
        {
            if (node->middleware_ctx[i])
            {
                MiddlewareInfo *middleware_info = (MiddlewareInfo *)node->middleware_ctx[i];
                if (middleware_info)
                {
                    free_middleware_info(middleware_info);
                }
            }
        }
    }

    // Free param name
    if (node->param_name)
    {
        free(node->param_name);
    }

    free(node);
}

static inline int char_equal_ci(char a, char b)
{
    // Convert both to uppercase for comparison
    if (a >= 'a' && a <= 'z')
        a -= 32;
    if (b >= 'a' && b <= 'z')
        b -= 32;
    return a == b;
}

static inline int method_equal(const char *method, const char *expected, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (!char_equal_ci(method[i], expected[i]))
        {
            return 0;
        }
    }
    return method[len] == '\0'; // Ensure exact length match
}

// Get HTTP method index from string
http_method_t get_method_index(const char *method)
{
    if (!method || !method[0])
        return METHOD_UNKNOWN;

    // Fast path: switch on first character (most distinctive)
    char first = method[0];
    if (first >= 'a' && first <= 'z')
        first -= 32; // Convert to uppercase

    switch (first)
    {
    case 'G':
        // Only GET starts with G - quick 3-char check
        if (!method[1] || !method[2])
            return METHOD_UNKNOWN; // Safety check

        return (char_equal_ci(method[1], 'E') &&
                char_equal_ci(method[2], 'T') &&
                method[3] == '\0')
                   ? METHOD_GET
                   : METHOD_UNKNOWN;

    case 'P':
        // POST, PUT, PATCH start with P - check second char
        if (!method[1])
            return METHOD_UNKNOWN; // Safety check

        switch (method[1] | 32) // Convert to lowercase
        {
        case 'o':
            // POST - 4 characters
            if (!method[2] || !method[3])
                return METHOD_UNKNOWN; // Safety check
            return (char_equal_ci(method[2], 'S') &&
                    char_equal_ci(method[3], 'T') &&
                    method[4] == '\0')
                       ? METHOD_POST
                       : METHOD_UNKNOWN;

        case 'u':
            // PUT - 3 characters
            if (!method[2])
                return METHOD_UNKNOWN; // Safety check
            return (char_equal_ci(method[2], 'T') &&
                    method[3] == '\0')
                       ? METHOD_PUT
                       : METHOD_UNKNOWN;

        case 'a':
            // PATCH - 5 characters
            if (!method[2] || !method[3] || !method[4])
                return METHOD_UNKNOWN; // Safety check
            return (char_equal_ci(method[2], 'T') &&
                    char_equal_ci(method[3], 'C') &&
                    char_equal_ci(method[4], 'H') &&
                    method[5] == '\0')
                       ? METHOD_PATCH
                       : METHOD_UNKNOWN;
        }
        return METHOD_UNKNOWN;

    case 'D':
        // Only DELETE starts with D
        return method_equal(method, "DELETE", 6) ? METHOD_DELETE : METHOD_UNKNOWN;

    case 'H':
        // Only HEAD starts with H
        return method_equal(method, "HEAD", 4) ? METHOD_HEAD : METHOD_UNKNOWN;

    case 'O':
        // Only OPTIONS starts with O
        return method_equal(method, "OPTIONS", 7) ? METHOD_OPTIONS : METHOD_UNKNOWN;

    default:
        return METHOD_UNKNOWN;
    }
}

// Find a matching route
bool route_trie_match(route_trie_t *trie,
                      const char *method,
                      const tokenized_path_t *tokenized_path,
                      route_match_t *match)
{
    if (!trie || !method || !tokenized_path || !match)
        return false;

    http_method_t method_idx = get_method_index(method);
    if (method_idx == METHOD_UNKNOWN)
        return false;

    uv_rwlock_rdlock(&trie->lock);

    // Initialize match result
    match->handler = NULL;
    match->middleware_ctx = NULL;
    match->param_count = 0;

    trie_node_t *matched_node = NULL;

    // Handle root path
    if (tokenized_path->count == 0)
    {
        if (trie->root->is_end)
        {
            matched_node = trie->root;
        }
    }
    else
    {
        // Start from root/slash
        trie_node_t *start_node = trie->root;
        unsigned char sep = '/';
        if (start_node->children[sep])
        {
            start_node = start_node->children[sep];
        }

        matched_node = match_segments(start_node, tokenized_path, 0, match, 0);
    }

    // Extract handler if found
    if (matched_node && matched_node->handlers[method_idx])
    {
        match->handler = matched_node->handlers[method_idx];
        match->middleware_ctx = matched_node->middleware_ctx[method_idx];
        uv_rwlock_rdunlock(&trie->lock);
        return true;
    }

    uv_rwlock_rdunlock(&trie->lock);
    return false;
}

// Create a new route trie
route_trie_t *route_trie_create(void)
{
    route_trie_t *trie = calloc(1, sizeof(route_trie_t));
    if (!trie)
        return NULL;

    trie->root = trie_node_create();
    if (!trie->root)
    {
        free(trie);
        return NULL;
    }

    // Initialize read-write lock for thread safety
    if (uv_rwlock_init(&trie->lock) != 0)
    {
        trie_node_free(trie->root);
        free(trie);
        return NULL;
    }

    return trie;
}

// Add a route to the trie
int route_trie_add(route_trie_t *trie, const char *method, const char *path,
                   RequestHandler handler, void *middleware_ctx)
{
    if (!trie || !method || !path || !handler)
        return -1;

    http_method_t method_idx = get_method_index(method);
    if (method_idx == METHOD_UNKNOWN)
        return -1;

    // Write lock for thread safety
    uv_rwlock_wrlock(&trie->lock);

    trie_node_t *current = trie->root;
    const char *p = path;

    // Skip leading slash
    if (*p == '/')
        p++;

    while (*p)
    {
        // Handle parameter segments (:param)
        if (*p == ':')
        {
            p++; // Skip ':'

            // Extract parameter name
            const char *param_start = p;
            while (*p && *p != '/')
                p++;

            size_t param_len = p - param_start;

            // Create or navigate to param child
            if (!current->param_child)
            {
                current->param_child = trie_node_create();
                if (!current->param_child)
                {
                    uv_rwlock_wrunlock(&trie->lock);
                    return -1;
                }

                // Store parameter name
                current->param_child->param_name = malloc(param_len + 1);
                if (!current->param_child->param_name)
                {
                    uv_rwlock_wrunlock(&trie->lock);
                    return -1;
                }

                memcpy(current->param_child->param_name, param_start, param_len);
                current->param_child->param_name[param_len] = '\0';
            }

            current = current->param_child;
        }
        // Handle wildcard segments (*)
        else if (*p == '*')
        {
            if (!current->wildcard_child)
            {
                current->wildcard_child = trie_node_create();
                if (!current->wildcard_child)
                {
                    uv_rwlock_wrunlock(&trie->lock);
                    return -1;
                }
            }

            current = current->wildcard_child;
            break; // Wildcard matches everything after
        }
        // Handle regular segments
        else
        {
            // Process until next segment or end
            while (*p && *p != '/')
            {
                unsigned char c = (unsigned char)*p;

                // Create child if doesn't exist
                if (!current->children[c])
                {
                    current->children[c] = trie_node_create();
                    if (!current->children[c])
                    {
                        uv_rwlock_wrunlock(&trie->lock);
                        return -1;
                    }
                }

                current = current->children[c];
                p++;
            }
        }

        // Skip segment separator
        if (*p == '/')
        {
            unsigned char c = (unsigned char)'/';
            if (!current->children[c])
            {
                current->children[c] = trie_node_create();
                if (!current->children[c])
                {
                    uv_rwlock_wrunlock(&trie->lock);
                    return -1;
                }
            }
            current = current->children[c];
            p++;
        }
    }

    // Mark as end node and store handler
    current->is_end = true;
    current->handlers[method_idx] = handler;
    current->middleware_ctx[method_idx] = middleware_ctx;
    trie->route_count++;

    uv_rwlock_wrunlock(&trie->lock);
    return 0;
}

// Free the route trie
void route_trie_free(route_trie_t *trie)
{
    if (!trie)
        return;

    uv_rwlock_wrlock(&trie->lock);
    trie_node_free(trie->root);
    trie->root = NULL;
    uv_rwlock_wrunlock(&trie->lock);

    uv_rwlock_destroy(&trie->lock);
    free(trie);
}
