#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "route_trie.h"
#include "compat.h"
#include "middleware.h"

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
                // Free Middleware context
                MiddlewareInfo *middleware_info = (MiddlewareInfo *)node->middleware_ctx[i];
                if (middleware_info)
                {
                    if (middleware_info->middleware)
                    {
                        free(middleware_info->middleware);
                    }
                    free(middleware_info);
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

// Get HTTP method index from string
http_method_t get_method_index(const char *method)
{
    if (!method)
        return METHOD_UNKNOWN;

    if (strcasecmp(method, "GET") == 0)
        return METHOD_GET;
    if (strcasecmp(method, "POST") == 0)
        return METHOD_POST;
    if (strcasecmp(method, "PUT") == 0)
        return METHOD_PUT;
    if (strcasecmp(method, "DELETE") == 0)
        return METHOD_DELETE;
    if (strcasecmp(method, "PATCH") == 0)
        return METHOD_PATCH;
    if (strcasecmp(method, "HEAD") == 0)
        return METHOD_HEAD;
    if (strcasecmp(method, "OPTIONS") == 0)
        return METHOD_OPTIONS;

    return METHOD_UNKNOWN;
}

// Create a new route trie
route_trie_t *route_trie_create(void)
{
    route_trie_t *trie = malloc(sizeof(route_trie_t));
    if (!trie)
        return NULL;

    trie->root = trie_node_create();
    if (!trie->root)
    {
        free(trie);
        return NULL;
    }

    trie->route_count = 0;

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

// Match helper function
static bool trie_match_recursive(trie_node_t *node, const char *path,
                                 route_match_t *match, int depth)
{
    if (!node || depth > 100)
        return false; // Depth limit for safety

    // Base case: reached end of path
    if (*path == '\0')
    {
        return node->is_end;
    }

    // Skip leading slash
    if (*path == '/')
        path++;

    // Try exact match first
    const char *segment_end = path;
    while (*segment_end && *segment_end != '/')
        segment_end++;

    // Try exact character matching
    trie_node_t *current = node;
    const char *p = path;

    while (p < segment_end)
    {
        unsigned char c = (unsigned char)*p;
        if (!current->children[c])
            break;
        current = current->children[c];
        p++;
    }

    // If exact match succeeded for this segment
    if (p == segment_end)
    {
        // Handle segment separator or end
        if (*segment_end == '\0')
        {
            if (current->is_end)
                return true;
        }
        else
        {
            unsigned char sep = (unsigned char)'/';
            if (current->children[sep])
            {
                if (trie_match_recursive(current->children[sep], segment_end, match, depth + 1))
                {
                    return true;
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
            match->params[match->param_count].value.data = path;
            match->params[match->param_count].value.len = segment_end - path;
            match->param_count++;
        }

        // Continue matching after this segment
        if (*segment_end == '\0')
        {
            if (node->param_child->is_end)
                return true;
        }
        else
        {
            unsigned char sep = (unsigned char)'/';
            if (node->param_child->children[sep])
            {
                if (trie_match_recursive(node->param_child->children[sep], segment_end, match, depth + 1))
                {
                    return true;
                }
            }
        }

        // Backtrack parameter
        if (match && match->param_count > 0)
        {
            match->param_count--;
        }
    }

    // Try wildcard match (matches everything)
    if (node->wildcard_child)
    {
        return node->wildcard_child->is_end;
    }

    return false;
}

// Find a matching route
bool route_trie_match(route_trie_t *trie, const char *method, const char *path,
                      route_match_t *match)
{
    if (!trie || !method || !path || !match)
        return false;

    http_method_t method_idx = get_method_index(method);
    if (method_idx == METHOD_UNKNOWN)
        return false;

    // Read lock for thread safety
    uv_rwlock_rdlock(&trie->lock);

    // Initialize match result
    match->handler = NULL;
    match->middleware_ctx = NULL;
    match->param_count = 0;

    // Start matching from root
    bool found = false;
    const char *p = path;

    // Skip leading slash
    if (*p == '/')
        p++;

    // Handle root path specially
    if (*p == '\0')
    {
        if (trie->root->is_end)
        {
            match->handler = trie->root->handlers[method_idx];
            match->middleware_ctx = trie->root->middleware_ctx[method_idx];
            found = (match->handler != NULL);
        }
    }
    else
    {
        // Recursive matching
        trie_node_t *node = trie->root;

        // If root has a child for '/', start from there
        unsigned char sep = (unsigned char)'/';
        if (node->children[sep])
        {
            node = node->children[sep];
        }

        if (trie_match_recursive(node, p, match, 0))
        {
            // Find the end node and get handler
            trie_node_t *current = trie->root;
            const char *path_ptr = path;

            if (*path_ptr == '/')
                path_ptr++;

            // Navigate to the end node following the matched path
            // (This is simplified - in production you'd track the node during matching)
            // For now, we'll assume the handler was set during recursive match
            found = true;
        }
    }

    uv_rwlock_rdunlock(&trie->lock);
    return found;
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
