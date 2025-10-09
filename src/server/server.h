#ifndef ECEWO_SERVER_H
#define ECEWO_SERVER_H

#include "../../include/ecewo.h"
#include "client.h"

// Internal timer data (not exposed to users)
typedef struct timer_data_s
{
    timer_callback_t callback;
    void *user_data;
    int is_interval;
} timer_data_t;

#endif
