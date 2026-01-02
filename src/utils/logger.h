#ifndef ECEWO_LOGGER_H
#define ECEWO_LOGGER_H

#include <stdio.h>

#ifdef ECEWO_DEBUG
#define LOG_DEBUG(fmt, ...) \
  fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#define LOG_ERROR(fmt, ...) \
  fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

#endif
