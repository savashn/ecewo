/**
 * Platform compatibility macros and functions
 */

#ifndef ECEWO_COMPAT_H
#define ECEWO_COMPAT_H

#ifdef _WIN32
#include <string.h>
#define strcasecmp _stricmp
#define strdup _strdup
#endif

#endif
