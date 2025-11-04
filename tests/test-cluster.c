#include "unity.h"
#include "ecewo.h"
#include "ecewo/mock.h"
#include "ecewo/cluster.h"
#include <string.h>
#include <stdio.h>

#define REQUEST_COUNT 5

static bool worker_started = false;
static bool worker_exited = false;
static uint8_t last_worker_id = 0;
static int last_exit_status = 0;

void test_worker_start_callback(uint8_t worker_id)
{
    worker_started = true;
    last_worker_id = worker_id;
}

void test_worker_exit_callback(uint8_t worker_id, int status)
{
    worker_exited = true;
    last_worker_id = worker_id;
    last_exit_status = status;
}

// ============================================================================
// TESTS
// ============================================================================

void test_cluster_cpu_count(void)
{
    uint8_t cpu_count = cluster_cpu_count();
    
    TEST_ASSERT_GREATER_THAN(1, cpu_count);
    TEST_ASSERT_LESS_OR_EQUAL(255, cpu_count);
}

void test_cluster_callbacks(void)
{
    worker_started = false;
    worker_exited = false;
    
    Cluster config = {
        .workers = 2,
        .respawn = true,
        .port = 3000,
        .on_start = test_worker_start_callback,
        .on_exit = test_worker_exit_callback
    };
    
    TEST_ASSERT_NOT_NULL(config.on_start);
    TEST_ASSERT_NOT_NULL(config.on_exit);
}

void test_cluster_invalid_config(void)
{
    Cluster* null_config = NULL;

    bool init_result = cluster_init(null_config, 0, NULL);
    TEST_ASSERT_FALSE(init_result);

    Cluster invalid_workers = {
        .workers = 0,
        .port = 3000
    };

    init_result = cluster_init(&invalid_workers, 0, NULL);
    TEST_ASSERT_FALSE(init_result);

    Cluster invalid_port = {
        .workers = 2,
        .port = 0
    };

    init_result = cluster_init(&invalid_port, 0, NULL);
    TEST_ASSERT_FALSE(init_result);
}

#ifdef _WIN32
void test_cluster_windows_port_strategy(void)
{
    Cluster config = {
        .workers = 3,
        .respawn = true,
        .port = 3000
    };

    bool result = cluster_init(&config, 0, NULL);
    TEST_ASSERT_TRUE(result);

    uint16_t expected_ports[] = {3000, 3001, 3002};

    for (int i = 0; i < config.workers; i++) {
        uint16_t current_port = cluster_get_port();
        TEST_ASSERT_EQUAL(expected_ports[i], current_port);
    }

    TEST_ASSERT_EQUAL(3000, config.port);
    TEST_ASSERT_EQUAL(3, config.workers);
}
#else
void test_cluster_unix_port_strategy(void)
{
    Cluster config = {
        .workers = 4,
        .respawn = true,
        .port = 3000
    };
    
    TEST_ASSERT_EQUAL(3000, config.port);
    TEST_ASSERT_EQUAL(4, config.workers);
}
#endif

void test_cluster_config_defaults(void)
{
    Cluster config = {
        .workers = cluster_cpu_count(),
        .respawn = true,
        .port = 8080,
        .on_start = NULL,
        .on_exit = NULL
    };
    
    TEST_ASSERT_GREATER_THAN(1, config.workers);
    TEST_ASSERT_TRUE(config.respawn);
    TEST_ASSERT_EQUAL_INT16(8080, config.port);
    TEST_ASSERT_NULL(config.on_start);
    TEST_ASSERT_NULL(config.on_exit);
}

void test_cluster_workers_count(void)
{
    const uint8_t workers_count = 10;
    
    Cluster config = {
        .workers = workers_count,
        .respawn = false,
        .port = 3000
    };
    
    TEST_ASSERT_EQUAL_INT8(workers_count, config.workers);
}
