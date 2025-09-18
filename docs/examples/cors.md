# CORS

```c
// main.c

#include "ecewo.h"
#include <stdio.h>

void app_cleanup(void)
{
    cors_cleanup();
}

int main(void)
{
    if (server_init() != SERVER_OK)
    {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }

    cors_t my_cors = {
        .origin = "http://localhost:3000",        // Default "*"
        .methods = "GET, POST, OPTIONS",          // Default "GET, POST, PUT, DELETE, OPTIONS"
        .headers = "Content-Type, Authorization", // Default "Content-Type"
        .credentials = "true",                    // Default "false"
        .max_age = "86400",                       // Default "3600"
    };

    cors_init(&my_cors);  // Register CORS

    get("/", your_handler);

    shutdown_hook(app_cleanup);

    if (server_listen(3000) != SERVER_OK)
    {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }

    server_run();
    return 0;
}
```
