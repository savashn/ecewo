#include "ecewo.h"
#include "middleware.h"
#include <stdio.h>
#include <stdlib.h>

Router *routes = NULL;
size_t route_count = 0;
size_t routes_capacity = 0;

// Initialize router with default capacity
void init_router(void)
{
    routes_capacity = 10; // Start with space for 10 routes
    routes = (Router *)malloc(routes_capacity * sizeof(Router));
    if (routes == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate memory for routes\n");
        exit(EXIT_FAILURE);
    }
    route_count = 0;
}

// Cleanup function to free memory
void reset_router(void)
{
    if (routes != NULL)
    {
        // Clean up middleware information before freeing routes
        reset_middleware();

        free(routes);
        routes = NULL;
    }
    route_count = 0;
    routes_capacity = 0;
}
