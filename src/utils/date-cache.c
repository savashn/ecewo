#include "uv.h"
#include <stdbool.h>

typedef struct
{
    time_t timestamp;
    char date_str[64];
    uv_mutex_t mutex;
} date_cache_t;

static date_cache_t g_date_cache = {0};
static bool g_date_cache_initialized = false;

void init_date_cache(void)
{
    if (g_date_cache_initialized)
        return;
    
    uv_mutex_init(&g_date_cache.mutex);
    g_date_cache.timestamp = 0;
    g_date_cache_initialized = true;
}

void destroy_date_cache(void)
{
    if (!g_date_cache_initialized)
        return;
    
    uv_mutex_destroy(&g_date_cache.mutex);
    g_date_cache_initialized = false;
}

const char *get_cached_date(void)
{
    time_t now = time(NULL);
    
    // Fast path: cache hit (no lock needed)
    if (g_date_cache.timestamp == now)
        return g_date_cache.date_str;
    
    // Slow path: update cache
    uv_mutex_lock(&g_date_cache.mutex);
    
    // Double-check (başka thread update etmiş olabilir)
    if (g_date_cache.timestamp != now)
    {
        struct tm *gmt = gmtime(&now);
        strftime(g_date_cache.date_str, sizeof(g_date_cache.date_str),
                 "%a, %d %b %Y %H:%M:%S GMT", gmt);
        g_date_cache.timestamp = now;
    }
    
    uv_mutex_unlock(&g_date_cache.mutex);
    
    return g_date_cache.date_str;
}
