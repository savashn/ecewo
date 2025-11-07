#ifndef ECEWO_LOG_H
#define ECEWO_LOG_H

#include <stdio.h>

#ifndef NDEBUG
    #define LOG_DEBUG(fmt, ...) \
        fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...)  ((void)0)
#endif

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

#define LOG_SERVER(fmt, ...) \
    fprintf(stdout, fmt "\n", ##__VA_ARGS__)

#endif
