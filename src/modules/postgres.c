#include "ecewo/postgres.h"
#include "ecewo.h"
#include "uv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct pg_query_s
{
    char *sql;
    char **params;
    int param_count;
    pg_result_cb_t result_cb;
    void *data;
    pg_query_t *next;
};

struct pg_async_s
{
    PGconn *conn;
    void *data;

    int is_connected;
    int is_executing;
    char *error_message;

    pg_query_t *query_queue;
    pg_query_t *query_queue_tail;
    pg_query_t *current_query;

    int handle_initialized;
#ifdef _WIN32
    uv_timer_t timer;
#else
    uv_poll_t poll;
#endif
};

static void execute_next_query(PGquery *pg);
static void handle_error(PGquery *pg, const char *error);
static void pg_async_cancel(PGquery *pg);
static void pg_async_destroy(PGquery *pg);

#ifdef _WIN32
static void on_timer(uv_timer_t *handle);
static void on_timer_closed(uv_handle_t *handle);
#else
static void on_poll(uv_poll_t *handle, int status, int events);
static void on_poll_closed(uv_handle_t *handle);
#endif

// ============================================================================
// CLEANUP HELPERS
// ============================================================================

static void free_query(pg_query_t *query)
{
    if (!query)
        return;

    if (query->sql)
        free(query->sql);

    if (query->params)
    {
        for (int i = 0; i < query->param_count; i++)
        {
            if (query->params[i])
                free(query->params[i]);
        }
        free(query->params);
    }

    free(query);
}

static void free_pgquery(PGquery *pg)
{
    if (!pg)
        return;

    if (pg->error_message)
    {
        free(pg->error_message);
        pg->error_message = NULL;
    }

    free(pg);
}

// ============================================================================
// CLOSE CALLBACKS
// ============================================================================

#ifdef _WIN32
static void on_timer_closed(uv_handle_t *handle)
{
    if (!handle || !handle->data)
        return;

    PGquery *pg = (PGquery *)handle->data;
    free_pgquery(pg);
}
#else
static void on_poll_closed(uv_handle_t *handle)
{
    if (!handle || !handle->data)
        return;

    PGquery *pg = (PGquery *)handle->data;
    free_pgquery(pg);
}
#endif

// ============================================================================
// CANCEL AND DESTROY
// ============================================================================

static void pg_async_cancel(PGquery *pg)
{
    if (!pg)
        return;

    if (pg->is_executing && pg->handle_initialized)
    {
#ifdef _WIN32
        uv_timer_stop(&pg->timer);
        if (!uv_is_closing((uv_handle_t *)&pg->timer))
        {
            uv_close((uv_handle_t *)&pg->timer, NULL);
        }
#else
        uv_poll_stop(&pg->poll);
        if (!uv_is_closing((uv_handle_t *)&pg->poll))
        {
            uv_close((uv_handle_t *)&pg->poll, NULL);
        }
#endif
        pg->handle_initialized = 0;
    }

    if (pg->conn && pg->is_executing)
    {
        PGcancel *cancel = PQgetCancel(pg->conn);
        if (cancel)
        {
            char errbuf[256];
            PQcancel(cancel, errbuf, sizeof(errbuf));
            PQfreeCancel(cancel);
        }
    }

    pg->is_executing = 0;

    pg_query_t *query = pg->query_queue;
    while (query)
    {
        pg_query_t *next = query->next;
        free_query(query);
        query = next;
    }

    pg->query_queue = NULL;
    pg->query_queue_tail = NULL;
    pg->current_query = NULL;
}

static void pg_async_destroy(PGquery *pg)
{
    if (!pg)
        return;

    pg_async_cancel(pg);

    // If handle is still active, close it (will free in callback)
    if (pg->handle_initialized)
    {
#ifdef _WIN32
        if (!uv_is_closing((uv_handle_t *)&pg->timer))
        {
            uv_close((uv_handle_t *)&pg->timer, on_timer_closed);
        }
#else
        if (!uv_is_closing((uv_handle_t *)&pg->poll))
        {
            uv_close((uv_handle_t *)&pg->poll, on_poll_closed);
        }
#endif
        // Will be freed in close callback
    }
    else
    {
        // No active handle, safe to free immediately
        free_pgquery(pg);
    }
}

// ============================================================================
// ERROR HANDLING
// ============================================================================

static void handle_error(PGquery *pg, const char *error)
{
    printf("handle_error: %s\n", error ? error : "Unknown error");

    if (error && !pg->error_message)
    {
        pg->error_message = strdup(error);
    }

    if (pg->is_executing)
    {
        pg->is_executing = 0;
        decrement_async_work();
    }

    pg_async_destroy(pg);
}

// ============================================================================
// LIBUV CALLBACKS
// ============================================================================

#ifdef _WIN32
static void on_timer(uv_timer_t *handle)
{
    if (!handle || !handle->data)
    {
        printf("on_timer: Invalid handle or handle->data\n");
        return;
    }

    PGquery *pg = (PGquery *)handle->data;

    if (!server_is_running())
    {
        uv_timer_stop(&pg->timer);
        if (!uv_is_closing((uv_handle_t *)&pg->timer))
        {
            uv_close((uv_handle_t *)&pg->timer, on_timer_closed);
        }
        pg->handle_initialized = 0;
        pg->is_executing = 0;
        decrement_async_work();
        return;
    }

    if (!PQconsumeInput(pg->conn))
    {
        printf("on_timer: PQconsumeInput failed: %s\n", PQerrorMessage(pg->conn));
        handle_error(pg, PQerrorMessage(pg->conn));
        return;
    }

    if (PQisBusy(pg->conn))
    {
        return;
    }

    uv_timer_stop(&pg->timer);

    PGresult *result;
    while ((result = PQgetResult(pg->conn)) != NULL)
    {
        if (pg->current_query && pg->current_query->result_cb)
        {
            pg->current_query->result_cb(pg, result, pg->current_query->data);
        }

        ExecStatusType result_status = PQresultStatus(result);

        if (result_status != PGRES_TUPLES_OK && result_status != PGRES_COMMAND_OK)
        {
            const char *error_msg = PQresultErrorMessage(result);
            printf("on_timer: Query error: %s\n", error_msg);
            PQclear(result);
            pg->current_query = NULL;
            handle_error(pg, error_msg);
            return;
        }

        PQclear(result);
    }

    if (pg->current_query)
    {
        free_query(pg->current_query);
        pg->current_query = NULL;
    }

    execute_next_query(pg);
}
#else
static void on_poll(uv_poll_t *handle, int status, int events)
{
    if (!handle || !handle->data)
    {
        printf("on_poll: Invalid handle or handle->data\n");
        return;
    }

    PGquery *pg = (PGquery *)handle->data;

    if (!server_is_running())
    {
        uv_poll_stop(&pg->poll);
        if (!uv_is_closing((uv_handle_t *)&pg->poll))
        {
            uv_close((uv_handle_t *)&pg->poll, on_poll_closed);
        }
        pg->handle_initialized = 0;
        pg->is_executing = 0;
        decrement_async_work();
        return;
    }

    if (status < 0)
    {
        printf("on_poll: Poll error: %s\n", uv_strerror(status));
        handle_error(pg, uv_strerror(status));
        return;
    }

    if (!PQconsumeInput(pg->conn))
    {
        printf("on_poll: PQconsumeInput failed: %s\n", PQerrorMessage(pg->conn));
        handle_error(pg, PQerrorMessage(pg->conn));
        return;
    }

    if (PQisBusy(pg->conn))
    {
        return;
    }

    uv_poll_stop(&pg->poll);

    PGresult *result;
    while ((result = PQgetResult(pg->conn)) != NULL)
    {
        if (pg->current_query && pg->current_query->result_cb)
        {
            pg->current_query->result_cb(pg, result, pg->current_query->data);
        }

        ExecStatusType result_status = PQresultStatus(result);

        if (result_status != PGRES_TUPLES_OK && result_status != PGRES_COMMAND_OK)
        {
            const char *error_msg = PQresultErrorMessage(result);
            printf("on_poll: Query error: %s\n", error_msg);
            PQclear(result);
            pg->current_query = NULL;
            handle_error(pg, error_msg);
            return;
        }

        PQclear(result);
    }

    if (pg->current_query)
    {
        free_query(pg->current_query);
        pg->current_query = NULL;
    }

    execute_next_query(pg);
}
#endif

// ============================================================================
// QUERY EXECUTION
// ============================================================================

static void execute_next_query(PGquery *pg)
{
    if (!pg->query_queue)
    {
        pg->is_executing = 0;
        decrement_async_work();

        pg_async_destroy(pg);
        return;
    }

    if (!server_is_running())
    {
        pg->is_executing = 0;
        decrement_async_work();
        pg_async_destroy(pg);
        return;
    }

    pg->current_query = pg->query_queue;
    pg->query_queue = pg->query_queue->next;
    if (!pg->query_queue)
    {
        pg->query_queue_tail = NULL;
    }

    int result;
    if (pg->current_query->param_count > 0)
    {
        result = PQsendQueryParams(
            pg->conn,
            pg->current_query->sql,
            pg->current_query->param_count,
            NULL, // param types
            (const char **)pg->current_query->params,
            NULL, // param lengths
            NULL, // param formats
            0);   // result format (text)
    }
    else
    {
        result = PQsendQuery(pg->conn, pg->current_query->sql);
    }

    if (!result)
    {
        printf("execute_next_query: Failed to send query: %s\n",
               PQerrorMessage(pg->conn));
        handle_error(pg, PQerrorMessage(pg->conn));
        return;
    }

    // Initialize and start libuv handle
#ifdef _WIN32
    if (!pg->handle_initialized)
    {
        int init_result = uv_timer_init(uv_default_loop(), &pg->timer);
        if (init_result != 0)
        {
            printf("execute_next_query: uv_timer_init failed: %s\n",
                   uv_strerror(init_result));
            handle_error(pg, uv_strerror(init_result));
            return;
        }
        pg->handle_initialized = 1;
        pg->timer.data = pg;
    }

    int start_result = uv_timer_start(&pg->timer, on_timer, 10, 10);
    if (start_result != 0)
    {
        printf("execute_next_query: uv_timer_start failed: %s\n",
               uv_strerror(start_result));
        handle_error(pg, uv_strerror(start_result));
        return;
    }
#else
    int sock = PQsocket(pg->conn);

    if (sock < 0)
    {
        printf("execute_next_query: Invalid socket: %d\n", sock);
        handle_error(pg, "Invalid PostgreSQL socket");
        return;
    }

    if (!pg->handle_initialized)
    {
        int init_result = uv_poll_init(uv_default_loop(), &pg->poll, sock);
        if (init_result != 0)
        {
            printf("execute_next_query: uv_poll_init failed: %s\n",
                   uv_strerror(init_result));
            handle_error(pg, uv_strerror(init_result));
            return;
        }

        pg->handle_initialized = 1;
        pg->poll.data = pg;
    }

    int start_result = uv_poll_start(&pg->poll, UV_READABLE | UV_WRITABLE, on_poll);
    if (start_result != 0)
    {
        printf("execute_next_query: uv_poll_start failed: %s\n",
               uv_strerror(start_result));
        handle_error(pg, uv_strerror(start_result));
        return;
    }
#endif
}

// ============================================================================
// PUBLIC API
// ============================================================================

PGquery *query_create(PGconn *existing_conn, void *data)
{
    if (!existing_conn)
    {
        printf("query_create: existing_conn is NULL\n");
        return NULL;
    }

    if (PQstatus(existing_conn) != CONNECTION_OK)
    {
        printf("query_create: Connection status is not OK: %d\n",
               PQstatus(existing_conn));
        return NULL;
    }

    PGquery *pg = malloc(sizeof(PGquery));
    if (!pg)
    {
        printf("query_create: Failed to allocate memory\n");
        return NULL;
    }

    memset(pg, 0, sizeof(PGquery));
    pg->conn = existing_conn;
    pg->data = data;
    pg->is_connected = 1;
    pg->is_executing = 0;
    pg->handle_initialized = 0;
    pg->error_message = NULL;
    pg->query_queue = NULL;
    pg->query_queue_tail = NULL;
    pg->current_query = NULL;

#ifdef _WIN32
    pg->timer.data = pg;
#else
    pg->poll.data = pg;
#endif

    return pg;
}

int query_queue(PGquery *pg,
                const char *sql,
                int param_count,
                const char **params,
                pg_result_cb_t result_cb,
                void *query_data)
{
    if (!pg || !sql)
    {
        printf("query_queue: Invalid parameters\n");
        return -1;
    }

    pg_query_t *query = malloc(sizeof(pg_query_t));
    if (!query)
    {
        printf("query_queue: Failed to allocate query\n");
        return -1;
    }

    memset(query, 0, sizeof(pg_query_t));
    query->next = NULL;

    query->sql = strdup(sql);
    if (!query->sql)
    {
        printf("query_queue: Failed to copy SQL\n");
        free(query);
        return -1;
    }

    if (param_count > 0 && params)
    {
        query->params = malloc(param_count * sizeof(char *));
        if (!query->params)
        {
            printf("query_queue: Failed to allocate params\n");
            free(query->sql);
            free(query);
            return -1;
        }

        for (int i = 0; i < param_count; i++)
        {
            if (params[i])
            {
                query->params[i] = strdup(params[i]);

                if (!query->params[i])
                {
                    printf("query_queue: Failed to allocate a param\n");

                    for (int j = 0; j < i; j++)
                    {
                        free(query->params[j]);
                    }

                    free(query->params);
                    free(query->sql);
                    free(query);
                    return -1
                }
            }
            else
            {
                query->params[i] = NULL;
            }
        }
    }
    else
    {
        query->params = NULL;
    }

    query->param_count = param_count;
    query->result_cb = result_cb;
    query->data = query_data;

    if (!pg->query_queue)
    {
        pg->query_queue = pg->query_queue_tail = query;
    }
    else
    {
        pg->query_queue_tail->next = query;
        pg->query_queue_tail = query;
    }

    return 0;
}

int query_execute(PGquery *pg)
{
    if (!pg)
    {
        printf("query_execute: pg is NULL\n");
        return -1;
    }

    if (!pg->is_connected)
    {
        printf("query_execute: Not connected\n");
        return -1;
    }

    if (pg->is_executing)
    {
        printf("query_execute: Already executing\n");
        return -1;
    }

    if (!pg->query_queue)
    {
        free_pgquery(pg);
        return 0;
    }

    increment_async_work();

    pg->is_executing = 1;
    execute_next_query(pg);
    return 0;
}
