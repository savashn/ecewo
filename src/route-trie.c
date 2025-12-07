#include <stdlib.h>
#include <inttypes.h>
#include "route-trie.h"
#include "middleware.h"

// Splits a path into segments (/users/123/posts -> ["users", "123", "posts"])
int tokenize_path(Arena *arena, const char *path, tokenized_path_t *result)
{
    if (!path || !result)
        return -1;

    memset(result, 0, sizeof(tokenized_path_t));

    // Skip leading slash
    if (*path == '/')
        path++;

    // Handle root path
    if (*path == '\0')
        return 0;

    uint8_t segment_count = 0;
    const char *p = path;
    while (*p)
    {
        if (*p != '/')
        {
            segment_count++;

            if (segment_count > MAX_PATH_SEGMENTS)
            {
                LOG_DEBUG("Path too deep: %" PRIu8 " segments (max %d)", segment_count, MAX_PATH_SEGMENTS);
                return -1;
            }

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

    result->capacity = segment_count;
    result->segments = arena_alloc(arena, sizeof(path_segment_t) * segment_count);
    if (!result->segments)
        return -1;

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
                                   uint8_t segment_idx,
                                   route_match_t *match,
                                   uint8_t depth)
{
    if (!node || depth > MAX_PATH_SEGMENTS)
        return NULL;

    // Path is completely finished
    if (segment_idx >= path->count)
        return node->is_end ? node : NULL;

    const path_segment_t *segment = &path->segments[segment_idx];
    trie_node_t *result = NULL;

    // Try exact (literal) match
    if (!segment->is_param && !segment->is_wildcard)
    {
        trie_node_t *current = node;

        for (size_t i = 0; i < segment->len && current; i++)
        {
            unsigned char c = (unsigned char)segment->start[i];
            current = current->children[c];
        }

        if (current)
        {
            if (segment_idx + 1 >= path->count)
            {
                // The last segment
                // Is the exact match is_end?
                if (current->is_end)
                    return current;
            }
            else
            {
                // There are more segments, continue
                unsigned char sep = '/';
                if (current->children[sep])
                {
                    result = match_segments(
                        current->children[sep], path, segment_idx + 1, match, depth + 1);
                    if (result)
                        return result;
                }
            }
        }
    }

    // Try param match
    if (node->param_child)
    {
        uint8_t original_param_count = match ? match->param_count : 0;

        // Add the param
        if (match && match->param_count < 32)
        {
            match->params[match->param_count].key.data = node->param_child->param_name;
            match->params[match->param_count].key.len = strlen(node->param_child->param_name);
            match->params[match->param_count].value.data = segment->start;
            match->params[match->param_count].value.len = segment->len;
            match->param_count++;
        }

        // Try the longer path first
        if (segment_idx + 1 < path->count)
        {
            unsigned char sep = '/';
            if (node->param_child->children[sep])
            {
                result = match_segments(
                    node->param_child->children[sep], path, segment_idx + 1, match, depth + 1);
                if (result)
                    return result;
            }
        }
        
        // There is no longer path
        // Is this node is_end and the path has finished?
        if (segment_idx + 1 >= path->count && node->param_child->is_end)
            return node->param_child;

        // None of them works, rollback
        if (match)
            match->param_count = original_param_count;
    }

    // Wildcard
    if (node->wildcard_child && node->wildcard_child->is_end)
        return node->wildcard_child;

    return NULL;
}

static trie_node_t *trie_node_create(void)
{
    trie_node_t *node = calloc(1, sizeof(trie_node_t));
    if (!node)
        return NULL;

    node->is_end = false;
    return node;
}

static void trie_node_free(trie_node_t *node)
{
    if (!node)
        return;

    for (uint8_t i = 0; i < 128; i++)
    {
        if (node->children[i])
        {
            trie_node_free(node->children[i]);
        }
    }

    if (node->param_child)
        trie_node_free(node->param_child);

    if (node->wildcard_child)
        trie_node_free(node->wildcard_child);

    if (node->is_end)
    {
        for (uint8_t i = 0; i < METHOD_COUNT; i++)
        {
            if (node->middleware_ctx[i])
            {
                MiddlewareInfo *middleware_info = (MiddlewareInfo *)node->middleware_ctx[i];
                if (middleware_info)
                    free_middleware_info(middleware_info);
            }
        }
    }

    if (node->param_name)
        free(node->param_name);

    free(node);
}

static int method_to_index(llhttp_method_t method)
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

bool route_trie_match(route_trie_t *trie,
                      llhttp_t *parser,
                      const tokenized_path_t *tokenized_path,
                      route_match_t *match)
{
    if (!trie || !parser || !tokenized_path || !match)
        return false;

    llhttp_method_t method = llhttp_get_method(parser);
    int method_idx = method_to_index(method);

    if (method_idx < 0)
        return false;

    match->handler = NULL;
    match->middleware_ctx = NULL;
    match->param_count = 0;

    trie_node_t *matched_node = NULL;

    if (tokenized_path->count == 0)
    {
        if (trie->root->is_end)
            matched_node = trie->root;
    }
    else
    {
        trie_node_t *start_node = trie->root;
        unsigned char sep = '/';
        if (start_node->children[sep])
            start_node = start_node->children[sep];

        matched_node = match_segments(start_node, tokenized_path, 0, match, 0);
    }

    if (matched_node && matched_node->handlers[method_idx])
    {
        match->handler = matched_node->handlers[method_idx];
        match->middleware_ctx = matched_node->middleware_ctx[method_idx];
        return true;
    }

    return false;
}

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

    return trie;
}

int route_trie_add(route_trie_t *trie,
                   llhttp_method_t method,
                   const char *path,
                   RequestHandler handler,
                   void *middleware_ctx)
{
    if (!trie || !path || !handler)
        return -1;

    int method_idx = method_to_index(method);
    if (method_idx < 0)
    {
        LOG_DEBUG("Unsupported HTTP method: %d", method);
        return -1;
    }

    trie_node_t *current = trie->root;
    const char *p = path;

    if (*p == '/')
        p++;

    while (*p)
    {
        if (*p == ':')
        {
            p++;

            const char *param_start = p;
            while (*p && *p != '/')
                p++;

            size_t param_len = p - param_start;

            if (!current->param_child)
            {
                current->param_child = trie_node_create();
                if (!current->param_child)
                    return -1;

                current->param_child->param_name = malloc(param_len + 1);
                if (!current->param_child->param_name)
                    return -1;

                memcpy(current->param_child->param_name, param_start, param_len);
                current->param_child->param_name[param_len] = '\0';
            }

            current = current->param_child;
        }
        else if (*p == '*')
        {
            if (!current->wildcard_child)
            {
                current->wildcard_child = trie_node_create();
                if (!current->wildcard_child)
                    return -1;
            }

            current = current->wildcard_child;
            break;
        }
        else
        {
            while (*p && *p != '/')
            {
                unsigned char c = (unsigned char)*p;

                if (!current->children[c])
                {
                    current->children[c] = trie_node_create();
                    if (!current->children[c])
                        return -1;
                }

                current = current->children[c];
                p++;
            }
        }

        if (*p == '/')
        {
            unsigned char c = (unsigned char)'/';
            if (!current->children[c])
            {
                current->children[c] = trie_node_create();
                if (!current->children[c])
                    return -1;
            }
            current = current->children[c];
            p++;
        }
    }

    current->is_end = true;
    current->handlers[method_idx] = handler;
    current->middleware_ctx[method_idx] = middleware_ctx;
    trie->route_count++;

    return 0;
}

void route_trie_free(route_trie_t *trie)
{
    if (!trie)
        return;

    trie_node_free(trie->root);
    trie->root = NULL;
    free(trie);
}
