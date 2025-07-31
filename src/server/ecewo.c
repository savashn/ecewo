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

// Helper function to expand the routes array when needed
void expand_routes(void)
{
    if (route_count >= routes_capacity)
    {
        size_t new_capacity = routes_capacity * 2; // Double the capacity

        // Check for potential integer overflow
        if (new_capacity < routes_capacity)
        {
            fprintf(stderr, "Error: Routes capacity overflow\n");
            exit(EXIT_FAILURE);
        }

        // Calculate the new size with error checking
        size_t new_size = new_capacity * sizeof(Router);
        if (new_size / sizeof(Router) != new_capacity)
        {
            fprintf(stderr, "Error: Routes size calculation overflow\n");
            exit(EXIT_FAILURE);
        }

        Router *new_routes = (Router *)realloc(routes, new_size);
        if (new_routes == NULL)
        {
            fprintf(stderr, "Error: Failed to reallocate memory for routes\n");
            return; // Return instead of exit to allow for error handling
        }

        routes = new_routes;
        routes_capacity = new_capacity;
    }
}