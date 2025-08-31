#include "ecewo.h"
#include "../lib/middleware.h"
#include <stdio.h>
#include <stdlib.h>

route_trie_t *global_route_trie = NULL;

// Initialize router with default capacity
void router_init(void)
{
    global_route_trie = route_trie_create();
    if (!global_route_trie)
    {
        fprintf(stderr, "Error: Failed to create route trie\n");
        exit(EXIT_FAILURE);
    }
}

void router_cleanup(void)
{
    if (global_route_trie)
    {
        // Middleware contexts will be cleaned up in route_trie_free
        route_trie_free(global_route_trie);
        global_route_trie = NULL;
    }

    reset_middleware();
}
