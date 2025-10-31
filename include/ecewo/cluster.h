#ifndef ECEWO_CLUSTER_H
#define ECEWO_CLUSTER_H

#include <stdbool.h>
#include <stdint.h>
#include "ecewo-config.h"

#ifndef ECEWO_HAS_CLUSTER
#error "Cluster module is not enabled. Build with -DECEWO_CLUSTER=ON"
#endif

typedef struct
{
    uint8_t workers;
    bool respawn;
    void (*on_worker_start)(uint8_t  worker_id);
    void (*on_worker_exit)(uint8_t  worker_id, int status);
} Cluster;

bool cluster_init(const Cluster *config, uint16_t base_port, int argc, char **argv);
uint16_t cluster_get_port(void);
bool cluster_is_master(void);
bool cluster_is_worker(void);
uint8_t cluster_worker_id(void);
uint8_t cluster_worker_count(void);
uint8_t cluster_cpu_count(void);
void cluster_signal_workers(int signal);
void cluster_wait_workers(void);

#endif
