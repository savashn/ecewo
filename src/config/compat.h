#ifndef COMPAT_H
#define COMPAT_H

#ifdef _WIN32
#define strcasecmp _stricmp
#define strdup _strdup
#define strncasecmp _strnicmp
#endif

#endif
