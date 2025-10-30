#ifndef ECEWO_CLUSTER_H
#define ECEWO_CLUSTER_H

#include <stdbool.h>
#include "ecewo-config.h"

#ifndef ECEWO_HAS_CLUSTER
#error "Cluster module is not enabled. Build with -DECEWO_CLUSTER=ON"
#endif

typedef struct
{
    int workers;
    bool respawn;
    void (*on_worker_start)(int worker_id);
    void (*on_worker_exit)(int worker_id, int status);
} Cluster;

bool cluster_init(const Cluster *config, int base_port, int argc, char **argv);
int cluster_get_port(void);
bool cluster_is_master(void);
bool cluster_is_worker(void);
int cluster_worker_id(void);
int cluster_worker_count(void);
int cluster_cpu_count(void);
void cluster_signal_workers(int signal);
void cluster_wait_workers(void);

#endif
