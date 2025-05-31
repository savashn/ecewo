#ifndef DOTENV_DOTENV_H
#define DOTENV_DOTENV_H

#include <stdbool.h>

#ifndef ENV
#define ENV "../../.env"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @param path Can be a directory containing a file named .env, or the path of the env file itself
     * @param overwrite Existing variables will be overwritten
     * @return 0 on success, -1 if can't open the file
     */
    int env_load(const char *path, bool overwrite);

#ifdef __cplusplus
}
#endif

#endif