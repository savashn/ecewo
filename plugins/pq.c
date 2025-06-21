#include "pq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Internal helper functions
#ifdef _WIN32
static void on_timer(uv_timer_t *handle);
#else
static void on_poll(uv_poll_t *handle, int status, int events);
#endif
static void execute_next_query(pg_async_t *pg);
static void cleanup_query(pg_query_t *query);
static void handle_error(pg_async_t *pg, const char *error);
static void pg_async_destroy(pg_async_t *pg);

// Create new async PostgreSQL context
pg_async_t *pq_create(PGconn *existing_conn, void *data)
{
    if (!existing_conn)
    {
        printf("pq_create: existing_conn is NULL\n");
        return NULL;
    }

    if (PQstatus(existing_conn) != CONNECTION_OK)
    {
        printf("pq_create: Connection status is not OK: %d\n", PQstatus(existing_conn));
        return NULL;
    }

    pg_async_t *pg = calloc(1, sizeof(pg_async_t));
    if (!pg)
    {
        printf("pq_create: Failed to allocate memory\n");
        return NULL;
    }

    // Use existing connection
    pg->conn = existing_conn;
    pg->owns_connection = 0;

    // Setup callbacks and user data
    pg->data = data;
    pg->is_connected = 1;

    // Initialize appropriate handle based on platform
#ifdef _WIN32
    memset(&pg->timer, 0, sizeof(pg->timer));
    pg->timer.data = pg;
#else
    memset(&pg->poll, 0, sizeof(pg->poll));
    pg->poll.data = pg;
#endif

    return pg;
}

// Add query to execution queue
int pq_queue(pg_async_t *pg,
             const char *sql,
             int param_count,
             const char **params,
             pg_result_cb_t result_cb,
             void *query_data)
{
    if (!pg || !sql)
    {
        printf("pq_queue: Invalid parameters\n");
        return -1;
    }

    pg_query_t *query = calloc(1, sizeof(pg_query_t));
    if (!query)
    {
        printf("pq_queue: Failed to allocate query\n");
        return -1;
    }

    // Copy SQL
    query->sql = strdup(sql);
    if (!query->sql)
    {
        printf("pq_queue: Failed to copy SQL\n");
        free(query);
        return -1;
    }

    // Copy parameters if any
    if (param_count > 0 && params)
    {
        query->params = malloc(param_count * sizeof(char *));
        if (!query->params)
        {
            printf("pq_queue: Failed to allocate params\n");
            free(query->sql);
            free(query);
            return -1;
        }

        for (int i = 0; i < param_count; i++)
        {
            query->params[i] = params[i] ? strdup(params[i]) : NULL;
        }
    }

    query->param_count = param_count;
    query->result_cb = result_cb;
    query->data = query_data;

    // Add to queue
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

// Start executing queued queries
int pq_execute(pg_async_t *pg)
{
    if (!pg)
    {
        printf("pq_execute: pg is NULL\n");
        return -1;
    }

    if (!pg->is_connected)
    {
        printf("pq_execute: Not connected\n");
        return -1;
    }

    if (pg->is_executing)
    {
        printf("pq_execute: Already executing\n");
        return -1;
    }

    if (!pg->query_queue)
    {
        // Always auto-cleanup when no queries remain
        pg_async_destroy(pg);
        return 0;
    }

    pg->is_executing = 1;
    execute_next_query(pg);
    return 0;
}

// Cancel current operations and cleanup
static void pg_async_cancel(pg_async_t *pg)
{
    if (!pg)
        return;

    if (pg->is_executing)
    {
#ifdef _WIN32
        uv_timer_stop(&pg->timer);
#else
        uv_poll_stop(&pg->poll);
#endif

        // Get cancel struct and send cancel request
        PGcancel *cancel = PQgetCancel(pg->conn);
        if (cancel)
        {
            char errbuf[256];
            PQcancel(cancel, errbuf, sizeof(errbuf));
            PQfreeCancel(cancel);
        }

        pg->is_executing = 0;
    }

    // Clear query queue
    pg_query_t *query = pg->query_queue;
    while (query)
    {
        pg_query_t *next = query->next;
        cleanup_query(query);
        query = next;
    }
    pg->query_queue = pg->query_queue_tail = NULL;
}

// Destroy context and free resources
static void pg_async_destroy(pg_async_t *pg)
{
    if (!pg)
        return;

    pg_async_cancel(pg);

    // Only close connection if we own it
    if (pg->conn && pg->owns_connection)
    {
        PQfinish(pg->conn);
    }

    if (pg->error_message)
    {
        free(pg->error_message);
    }

    free(pg);
}

// Execute the next query in the queue
static void execute_next_query(pg_async_t *pg)
{
    if (!pg->query_queue)
    {
        // All queries completed successfully
        pg->is_executing = 0;

        // Always auto-cleanup when all queries are done
        pg_async_destroy(pg);
        return;
    }

    pg->current_query = pg->query_queue;
    pg->query_queue = pg->query_queue->next;
    if (!pg->query_queue)
    {
        pg->query_queue_tail = NULL;
    }

    // Send query asynchronously
    int result;
    if (pg->current_query->param_count > 0)
    {
        result = PQsendQueryParams(pg->conn,
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
        printf("execute_next_query: Failed to send query: %s\n", PQerrorMessage(pg->conn));
        handle_error(pg, PQerrorMessage(pg->conn));
        return;
    }

#ifdef _WIN32

    int init_result = uv_timer_init(uv_default_loop(), &pg->timer);
    if (init_result != 0)
    {
        printf("execute_next_query: uv_timer_init failed: %s\n", uv_strerror(init_result));
        handle_error(pg, uv_strerror(init_result));
        return;
    }

    pg->timer.data = pg;

    int start_result = uv_timer_start(&pg->timer, on_timer, 10, 10);
    if (start_result != 0)
    {
        printf("execute_next_query: uv_timer_start failed: %s\n", uv_strerror(start_result));
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

    int init_result = uv_poll_init(uv_default_loop(), &pg->poll, sock);
    if (init_result != 0)
    {
        printf("execute_next_query: uv_poll_init failed: %s\n", uv_strerror(init_result));
        handle_error(pg, uv_strerror(init_result));
        return;
    }

    int start_result = uv_poll_start(&pg->poll, UV_READABLE | UV_WRITABLE, on_poll);
    if (start_result != 0)
    {
        printf("execute_next_query: uv_poll_start failed: %s\n", uv_strerror(start_result));
        handle_error(pg, uv_strerror(start_result));
        return;
    }

#endif
}

#ifdef _WIN32
// Timer callback for Windows
static void on_timer(uv_timer_t *handle)
{
    if (!handle || !handle->data)
    {
        printf("on_timer: Invalid handle or handle->data\n");
        return;
    }

    pg_async_t *pg = (pg_async_t *)handle->data;

    // Consume input from the connection
    if (!PQconsumeInput(pg->conn))
    {
        printf("on_timer: PQconsumeInput failed: %s\n", PQerrorMessage(pg->conn));
        uv_timer_stop(&pg->timer);
        handle_error(pg, PQerrorMessage(pg->conn));
        return;
    }

    // Check if we're still busy
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
            printf("on_timer: Calling result callback\n");
            pg->current_query->result_cb(pg, result, pg->current_query->data);
        }

        ExecStatusType result_status = PQresultStatus(result);

        if (result_status != PGRES_TUPLES_OK && result_status != PGRES_COMMAND_OK)
        {
            char *error_msg = strdup(PQresultErrorMessage(result));
            printf("on_timer: Query error: %s\n", error_msg);
            PQclear(result);
            cleanup_query(pg->current_query);
            pg->current_query = NULL;
            handle_error(pg, error_msg);
            free(error_msg);
            return;
        }

        PQclear(result);
    }

    cleanup_query(pg->current_query);
    pg->current_query = NULL;
    execute_next_query(pg);
}
#else
// Poll callback for Unix/Linux
static void on_poll(uv_poll_t *handle, int status, int events)
{
    if (!handle || !handle->data)
    {
        printf("on_poll: Invalid handle or handle->data\n");
        return;
    }

    pg_async_t *pg = (pg_async_t *)handle->data;

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
            char *error_msg = strdup(PQresultErrorMessage(result));
            printf("on_poll: Query error: %s\n", error_msg);
            PQclear(result);
            cleanup_query(pg->current_query);
            pg->current_query = NULL;
            handle_error(pg, error_msg);
            free(error_msg);
            return;
        }

        PQclear(result);
    }

    uv_poll_stop(&pg->poll);
    cleanup_query(pg->current_query);
    pg->current_query = NULL;
    execute_next_query(pg);
}
#endif

// Cleanup a query structure
static void cleanup_query(pg_query_t *query)
{
    if (!query)
        return;

    if (query->sql)
    {
        free(query->sql);
    }

    if (query->params)
    {
        for (int i = 0; i < query->param_count; i++)
        {
            if (query->params[i])
            {
                free(query->params[i]);
            }
        }
        free(query->params);
    }

    free(query);
}

// Handle error and cleanup
static void handle_error(pg_async_t *pg, const char *error)
{
    printf("handle_error: %s\n", error ? error : "Unknown error");

    if (pg->is_executing)
    {
#ifdef _WIN32
        uv_timer_stop(&pg->timer);
#else
        uv_poll_stop(&pg->poll);
#endif
        pg->is_executing = 0;
    }

    if (pg->error_message)
    {
        free(pg->error_message);
    }
    pg->error_message = error ? strdup(error) : NULL;

    // Cancel remaining queries
    pg_async_cancel(pg);

    // Always auto-cleanup on error
    printf("handle_error: An error occurred, automatically destroying context\n");
    pg_async_destroy(pg);
}
