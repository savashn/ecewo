#include "uv.h"
#include "ecewo.h"
#include "ecewo/cluster.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

#endif

#define MAX_WORKERS 255
#define MIN_WORKERS 1
#define RESPAWN_THROTTLE_COUNT 3
#define RESPAWN_THROTTLE_WINDOW 5 // seconds

typedef struct {
    uv_process_t handle;
    uint8_t worker_id;
    uint16_t port;
    bool active;
    
    time_t restart_times[RESPAWN_THROTTLE_COUNT];
    uint8_t restart_count;
    bool respawn_disabled;
    
    time_t start_time;
    int exit_status;
} worker_process_t;

static struct {
    bool is_master;
    uint8_t worker_id;
    uint8_t worker_count;
    uint16_t base_port;
    uint16_t worker_port;
    
    worker_process_t *workers;
    Cluster config;
    
    uv_signal_t sigchld;
    uv_signal_t sigterm;
    uv_signal_t sigint;
    uv_signal_t sigusr2;

    int original_argc;
    char **original_argv;
    char exe_path[1024];
    
    bool shutdown_requested;
    bool graceful_restart_requested;
    
    bool initialized;
} cluster_state = {0};

static void on_exit_cb(uv_process_t *handle, int64_t exit_status, int term_signal);
static void on_sigterm(uv_signal_t *handle, int signum);
static void on_sigint(uv_signal_t *handle, int signum);
#ifndef _WIN32
static void on_sigusr2(uv_signal_t *handle, int signum);
static void on_sigchld(uv_signal_t *handle, int signum);
#endif

static void save_original_args(int argc, char **argv)
{
    if (cluster_state.original_argv)
        return;
    
    cluster_state.original_argc = argc;
    
    cluster_state.original_argv = calloc(argc + 1, sizeof(char *));
    if (!cluster_state.original_argv)
    {
        fprintf(stderr, "ERROR: Failed to allocate memory for argv\n");
        return;
    }
    
    for (int i = 0; i < argc; i++)
    {
        if (argv[i])
        {
            cluster_state.original_argv[i] = strdup(argv[i]);
            if (!cluster_state.original_argv[i])
            {
                fprintf(stderr, "ERROR: Failed to duplicate argv[%d]\n", i);
                for (int j = 0; j < i; j++)
                    free(cluster_state.original_argv[j]);
                free(cluster_state.original_argv);
                cluster_state.original_argv = NULL;
                return;
            }
        }
    }
    cluster_state.original_argv[argc] = NULL;
    
    size_t size = sizeof(cluster_state.exe_path);
    if (uv_exepath(cluster_state.exe_path, &size) != 0)
    {
        fprintf(stderr, "Failed to get executable path\n");
        strncpy(cluster_state.exe_path, argv[0], sizeof(cluster_state.exe_path) - 1);
    }
}

static void cleanup_original_args(void)
{
    if (!cluster_state.original_argv)
        return;
        
    for (int i = 0; cluster_state.original_argv[i]; i++)
    {
        free(cluster_state.original_argv[i]);
    }
    free(cluster_state.original_argv);
    cluster_state.original_argv = NULL;
    cluster_state.original_argc = 0;
}

static char **build_worker_args(uint8_t worker_id, uint16_t port)
{
    if (!cluster_state.original_argv || cluster_state.original_argc == 0)
    {
        fprintf(stderr, "Original arguments not saved\n");
        return NULL;
    }
    
    int filtered_count = 0;
    for (int i = 0; i < cluster_state.original_argc; i++)
    {
        if (strcmp(cluster_state.original_argv[i], "--cluster-worker") == 0)
        {
            i += 2;
            continue;
        }

        filtered_count++;
    }
    
    // Total: filtered_args + --cluster-worker + id + port + NULL
    int total_argc = filtered_count + 3;
    char **args = calloc(total_argc + 1, sizeof(char *));
    if (!args)
    {
        fprintf(stderr, "ERROR: Failed to allocate worker args\n");
        return NULL;
    }
    
    int args_idx = 0;
    args[args_idx++] = cluster_state.exe_path;
    
    for (int i = 1; i < cluster_state.original_argc; i++)
    {
        if (strcmp(cluster_state.original_argv[i], "--cluster-worker") == 0)
        {
            i += 2; // Skip
            continue;
        }

        args[args_idx++] = cluster_state.original_argv[i];
    }
    
    char *worker_id_str = malloc(16);
    char *port_str = malloc(16);
    
    if (!worker_id_str || !port_str)
    {
        free(worker_id_str);
        free(port_str);
        free(args);
        fprintf(stderr, "ERROR: Failed to allocate worker arg strings\n");
        return NULL;
    }
    
    snprintf(worker_id_str, 16, "%u", (unsigned int)worker_id);
    snprintf(port_str, 16, "%u", (unsigned int)port);
    
    args[args_idx++] = "--cluster-worker";
    args[args_idx++] = worker_id_str;
    args[args_idx++] = port_str;
    args[args_idx] = NULL;
    
    return args;
}

static void free_worker_args(char **args)
{
    if (!args)
        return;
    
    for (int i = 0; args[i]; i++)
    {
        if (strcmp(args[i], "--cluster-worker") == 0)
        {
            if (args[i + 1]) free(args[i + 1]); // worker_id
            if (args[i + 2]) free(args[i + 2]); // port
            break;
        }
    }
    
    free(args);
}

static void setup_worker_stdio(uv_process_options_t *options)
{
    static uv_stdio_container_t stdio[3];
    
    stdio[0].flags = UV_IGNORE;
    
    stdio[1].flags = UV_INHERIT_FD;
    stdio[1].data.fd = 1;
    
    stdio[2].flags = UV_INHERIT_FD;
    stdio[2].data.fd = 2;
    
    options->stdio_count = 3;
    options->stdio = stdio;
}

static void load_env_config(Cluster *config)
{
    if (!config)
        return;

    char *env_workers = getenv("CLUSTER_WORKERS");
    if (env_workers)
    {
        int workers = atoi(env_workers);
        if (workers >= MIN_WORKERS && workers <= MAX_WORKERS)
            config->workers = (uint8_t)workers;
    }

    char *env_respawn = getenv("CLUSTER_RESPAWN");
    if (env_respawn)
    {
        config->respawn = (strcmp(env_respawn, "1") == 0 ||
                           strcmp(env_respawn, "true") == 0 ||
                           strcmp(env_respawn, "yes") == 0);
    }
}

static void apply_config(const Cluster *config)
{
    cluster_state.worker_count = MIN_WORKERS;
    cluster_state.config.respawn = false;
    cluster_state.config.on_start = NULL;
    cluster_state.config.on_exit = NULL;

    Cluster env_config = {0};
    load_env_config(&env_config);
    if (env_config.workers >= MIN_WORKERS)
        cluster_state.worker_count = env_config.workers;
    cluster_state.config.respawn = env_config.respawn;

    if (config)
    {
        if (config->workers >= MIN_WORKERS)
            cluster_state.worker_count = config->workers;

        cluster_state.config.respawn = config->respawn;
        cluster_state.config.on_start = config->on_start;
        cluster_state.config.on_exit = config->on_exit;
    }

    if (cluster_state.worker_count < MIN_WORKERS)
    {
        fprintf(stderr, "ERROR: Invalid worker count: %u (must be >= %d)\n",
                (unsigned int)cluster_state.worker_count, MIN_WORKERS);

        cluster_state.worker_count = MIN_WORKERS;
    }

    if (cluster_state.worker_count > MAX_WORKERS)
    {
        fprintf(stderr, "WARNING: Worker count %u exceeds max %d, capping\n",
                (unsigned int)cluster_state.worker_count, MAX_WORKERS);

        cluster_state.worker_count = MAX_WORKERS;
    }

    uint8_t cpu_count = cluster_cpu_count();
    if (cluster_state.worker_count > cpu_count * 2)
        fprintf(stderr, "WARNING: %u workers > 2x CPU count (%u) - may cause contention\n",
                (unsigned int)cluster_state.worker_count, (unsigned int)cpu_count);
}

static bool should_respawn_worker(worker_process_t *worker)
{
    if (!cluster_state.config.respawn || worker->respawn_disabled)
        return false;

    time_t now = time(NULL);
    
    if (worker->restart_count >= RESPAWN_THROTTLE_COUNT)
    {
        for (int i = 0; i < RESPAWN_THROTTLE_COUNT - 1; i++)
        {
            worker->restart_times[i] = worker->restart_times[i + 1];
        }

        worker->restart_count = RESPAWN_THROTTLE_COUNT - 1;
    }
    
    worker->restart_times[worker->restart_count++] = now;
    
    if (worker->restart_count >= RESPAWN_THROTTLE_COUNT)
    {
        time_t window = now - worker->restart_times[0];
        if (window < RESPAWN_THROTTLE_WINDOW)
        {
            fprintf(stderr, "ERROR: Worker %u crashing too fast (%d times in %lds), disabling respawn\n",
                    (unsigned int)worker->worker_id, RESPAWN_THROTTLE_COUNT, (long)window);

            worker->respawn_disabled = true;
            return false;
        }
    }
    
    return true;
}

static int spawn_worker(uint8_t worker_id, uint16_t port)
{
    if (worker_id >= cluster_state.worker_count)
    {
        fprintf(stderr, "ERROR: Invalid worker ID: %u\n", (unsigned int)worker_id);
        return -1;
    }
    
    if (!cluster_state.original_argv)
    {
        fprintf(stderr, "ERROR: Original arguments not saved\n");
        return -1;
    }
    
    worker_process_t *worker = &cluster_state.workers[worker_id];
    
    memset(worker, 0, sizeof(worker_process_t));
    worker->worker_id = worker_id;
    worker->port = port;
    worker->active = false;
    worker->respawn_disabled = false;
    worker->restart_count = 0;
    worker->start_time = time(NULL);
    
    char **args = build_worker_args(worker_id, port);
    if (!args)
    {
        fprintf(stderr, "Failed to build worker arguments\n");
        return -1;
    }
    
    uv_process_options_t options = {0};
    options.file = cluster_state.exe_path;
    options.args = args;
    options.exit_cb = on_exit_cb;
    
    setup_worker_stdio(&options);
    
#ifdef _WIN32
    options.flags = UV_PROCESS_WINDOWS_HIDE;
#else
    options.flags = UV_PROCESS_DETACHED;
#endif
    
    uv_process_t *handle = &worker->handle;
    handle->data = worker;
    
    int result = uv_spawn(uv_default_loop(), handle, &options);
    
    free_worker_args(args);
    
    if (result != 0)
    {
        fprintf(stderr, "Failed to spawn worker %u: %s\n", (unsigned int)worker_id, uv_strerror(result));
        return -1;
    }
    
    worker->active = true;
    
    if (cluster_state.config.on_start)
        cluster_state.config.on_start(worker_id);
    
    return 0;
}

static void on_exit_cb(uv_process_t *handle, int64_t exit_status, int term_signal)
{
    worker_process_t *worker = (worker_process_t *)handle->data;
    
    if (!worker || !cluster_state.is_master)
        return;
    
    uint8_t worker_id = worker->worker_id;
    time_t uptime = time(NULL) - worker->start_time;
    
    worker->active = false;
    worker->exit_status = (int)exit_status;
    
    bool is_crash = !cluster_state.shutdown_requested && exit_status != 0;
    
#ifndef _WIN32
    if (term_signal == SIGTERM || term_signal == SIGINT)
        is_crash = false;
#endif
    
    if (is_crash)
    {
        fprintf(stderr, "Worker %u crashed after %ld seconds (exit: %d",
                (unsigned int)worker_id, (long)uptime, (int)exit_status);
        
#ifndef _WIN32
        fprintf(stderr, ", signal: %d", term_signal);
#endif
        
        fprintf(stderr, ")\n");
    }
    
    if (cluster_state.config.on_exit)
        cluster_state.config.on_exit(worker_id, (int)exit_status);
    
    if (is_crash && should_respawn_worker(worker))
    {
        if (spawn_worker(worker_id, worker->port) != 0)
            fprintf(stderr, "Failed to respawn worker %u\n", (unsigned int)worker_id);
    }
    
    uv_close((uv_handle_t *)handle, NULL);
}

static void on_sigterm(uv_signal_t *handle, int signum)
{
    (void)handle;
    (void)signum;
    
    if (!cluster_state.is_master)
        return;
    
    cluster_state.shutdown_requested = true;
    
    for (uint8_t i = 0; i < cluster_state.worker_count; i++)
    {
        if (cluster_state.workers[i].active)
        {
            uv_process_kill(&cluster_state.workers[i].handle, SIGTERM);
        }
    }
}

static void on_sigint(uv_signal_t *handle, int signum)
{
    (void)handle;
    (void)signum;
    
    if (!cluster_state.is_master)
        return;
    
    cluster_state.shutdown_requested = true;
    
    for (uint8_t i = 0; i < cluster_state.worker_count; i++)
    {
        if (cluster_state.workers[i].active)
        {
            uv_process_kill(&cluster_state.workers[i].handle, SIGTERM);
        }
    }
}

#ifndef _WIN32
static void on_sigusr2(uv_signal_t *handle, int signum)
{
    (void)handle;
    (void)signum;
    
    if (!cluster_state.is_master)
        return;
    
    cluster_state.graceful_restart_requested = true;
    
    for (uint8_t i = 0; i < cluster_state.worker_count; i++)
    {
        if (cluster_state.workers[i].active)
            uv_process_kill(&cluster_state.workers[i].handle, SIGTERM);
    }
    
    cluster_state.graceful_restart_requested = false;
}

static void on_sigchld(uv_signal_t *handle, int signum)
{
    (void)handle;
    (void)signum;
    
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        // Already handled by on_exit_cb
    }
}
#endif

static void setup_signal_handlers(void)
{
    if (!cluster_state.is_master)
        return;
    
    uv_signal_init(uv_default_loop(), &cluster_state.sigterm);
    uv_signal_start(&cluster_state.sigterm, on_sigterm, SIGTERM);
    
    uv_signal_init(uv_default_loop(), &cluster_state.sigint);
    uv_signal_start(&cluster_state.sigint, on_sigint, SIGINT);
    
#ifndef _WIN32
    uv_signal_init(uv_default_loop(), &cluster_state.sigusr2);
    uv_signal_start(&cluster_state.sigusr2, on_sigusr2, SIGUSR2);
    
    uv_signal_init(uv_default_loop(), &cluster_state.sigchld);
    uv_signal_start(&cluster_state.sigchld, on_sigchld, SIGCHLD);
#endif
}

static void cleanup_signal_handlers(void)
{
    uv_signal_stop(&cluster_state.sigterm);
    uv_close((uv_handle_t *)&cluster_state.sigterm, NULL);
    
    uv_signal_stop(&cluster_state.sigint);
    uv_close((uv_handle_t *)&cluster_state.sigint, NULL);
    
#ifndef _WIN32
    uv_signal_stop(&cluster_state.sigusr2);
    uv_close((uv_handle_t *)&cluster_state.sigusr2, NULL);
    
    uv_signal_stop(&cluster_state.sigchld);
    uv_close((uv_handle_t *)&cluster_state.sigchld, NULL);
#endif
}

static void cluster_cleanup(void)
{
    if (!cluster_state.initialized)
        return;
    
    cleanup_signal_handlers();
    cleanup_original_args();
    
    if (cluster_state.workers)
    {
        free(cluster_state.workers);
        cluster_state.workers = NULL;
    }
    
    uv_loop_t *loop = uv_default_loop();
    while (uv_loop_alive(loop))
    {
        uv_run(loop, UV_RUN_NOWAIT);
    }
    uv_loop_close(loop);
    
    cluster_state.initialized = false;
}

bool cluster_init(const Cluster *config, int argc, char **argv)
{
    if (cluster_state.initialized)
    {
        fprintf(stderr, "Cluster already initialized\n");
        return false;
    }
    
    if (!config || config->workers == 0 || config->port == 0 || !argv)
    {
        fprintf(stderr, "Invalid cluster configuration\n");
        return false;
    }
    
    save_original_args(argc, argv);
    
    apply_config(config);
    cluster_state.base_port = config->port;
    
    char **args = uv_setup_args(argc, argv);
    
    cluster_state.is_master = true;
    cluster_state.worker_id = 0;
    
    for (int i = 1; args && i < argc - 2; i++)
    {
        if (strcmp(args[i], "--cluster-worker") == 0)
        {
            cluster_state.is_master = false;
            cluster_state.worker_id = (uint8_t)atoi(args[i + 1]);
            cluster_state.worker_port = (uint16_t)atoi(args[i + 2]);
            
            char title[64];
            snprintf(title, sizeof(title), "ecewo:worker-%u", 
                    (unsigned int)cluster_state.worker_id);
            uv_set_process_title(title);
            
            cluster_state.initialized = true;
            return false;  // Worker returns false
        }
    }
    
    // Master process
    static bool cleanup_registered = false;
    if (!cleanup_registered)
    {
        atexit(cluster_cleanup);
        cleanup_registered = true;
    }

    uv_set_process_title("ecewo:master");
    
    setup_signal_handlers();
    
    cluster_state.workers = calloc(config->workers, sizeof(worker_process_t));
    if (!cluster_state.workers)
    {
        fprintf(stderr, "Failed to allocate worker array\n");
        cleanup_original_args();
        return false;
    }
    
    int failed_count = 0;
    for (uint8_t i = 0; i < config->workers; i++)
    {
        uint16_t port;
        
#ifdef _WIN32
        port = config->port + i;
#else
        port = config->port;
#endif
        
        if (spawn_worker(i, port) != 0)
        {
            fprintf(stderr, "Failed to spawn worker %u\n", (unsigned int)i);
            failed_count++;
            
            if (failed_count > config->workers / 2)
            {
                fprintf(stderr, "Too many spawn failures, aborting\n");
                cleanup_original_args();
                return false;
            }
        }
        
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
    }
    
#ifdef _WIN32
    Sleep(500);
#else
    usleep(500000);
#endif
    
    cluster_state.initialized = true;
    return true;
}

uint16_t cluster_get_port(void)
{
    if (!cluster_state.initialized)
        return 0;
    
    if (cluster_state.is_master)
        return cluster_state.base_port;
    
    return cluster_state.worker_port;
}

bool cluster_is_master(void)
{
    return cluster_state.initialized && cluster_state.is_master;
}

bool cluster_is_worker(void)
{
    return cluster_state.initialized && !cluster_state.is_master;
}

uint8_t cluster_worker_id(void)
{
    return cluster_state.worker_id;
}

uint8_t cluster_worker_count(void)
{
    return cluster_state.worker_count;
}

uint8_t cluster_cpu_count(void)
{
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (uint8_t)sysinfo.dwNumberOfProcessors;
#else
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count > 255) count = 255;
    return count > 0 ? (uint8_t)count : 1;
#endif
}

void cluster_signal_workers(int signal)
{
    if (!cluster_state.is_master || !cluster_state.initialized)
    {
        fprintf(stderr, "Only master can signal workers\n");
        return;
    }
    
    for (uint8_t i = 0; i < cluster_state.worker_count; i++)
    {
        if (cluster_state.workers[i].active)
            uv_process_kill(&cluster_state.workers[i].handle, signal);
    }
}

void cluster_wait_workers(void)
{
    if (!cluster_state.is_master || !cluster_state.initialized)
    {
        fprintf(stderr, "Only master can wait for workers\n");
        return;
    }
    
    while (1)
    {
        bool any_active = false;
        for (uint8_t i = 0; i < cluster_state.worker_count; i++)
        {
            if (cluster_state.workers[i].active)
            {
                any_active = true;
                break;
            }
        }
        
        if (!any_active)
            break;
        
        uv_run(uv_default_loop(), UV_RUN_ONCE);
    }
    
    cluster_cleanup();
}
