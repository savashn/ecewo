# Async Postgres Queries

For asynchronous database queries, Ecewo supports [libpq](https://www.postgresql.org/docs/current/libpq.html) — the official PostgreSQL client library, which includes asynchronous support.

If you are using PostgreSQL, it is recommended to use [pquv](https://github.com/savashn/pquv) for async PostgreSQL queries. It basically combines libpq and libuv.

If you’re using another database, such as SQLite, you may still use the Ecewo's async API for performance-critical big queries.

## Installation

1. You need to install [PostgreSQL](https://www.postgresql.org/download/) first.
2. Configure your CMake as follows:

```cmake
find_package(PostgreSQL REQUIRED)

target_include_directories(server PRIVATE
    ${PostgreSQL_INCLUDE_DIRS}
)

target_link_libraries(server PRIVATE
    ecewo
    ${PostgreSQL_LIBRARIES}
)
```

3. Copy the `pquv.c` and `pquv.h` files from [the repository](https://github.com/savashn/pquv), paste them into your existing project anmd make the CMake configuration. Alternatively, if you have already installed [Ecewo-CLI](https://github.com/savashn/ecewo-cli), just run:

```
ecewo install pquv
ecewo rebuild dev
```

First of all, we need to configure and connect to our Postgres.

```c
// db.h

#ifndef DB_H
#define DB_H

#include <libpq-fe.h>

extern PGconn *db;

int db_init(void);
void db_close(void);

#endif
```

```c
// db.c

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "db.h"

PGconn *db = NULL;

static int create_tables(void)
{
    const char *query = 
        "CREATE TABLE IF NOT EXISTS users ("
        "  id SERIAL PRIMARY KEY, "
        "  name TEXT NOT NULL, "
        "  username TEXT UNIQUE NOT NULL, "
        "  password TEXT NOT NULL"
        ");";

    PGresult *res = PQexec(db, query);
    ExecStatusType status = PQresultStatus(res);
    
    if (status != PGRES_COMMAND_OK) {
        fprintf(stderr, "Table creation failed: %s\n", PQerrorMessage(db));
        PQclear(res);
        return 1;
    }
    
    PQclear(res);
    printf("Users table created or already exist.\n");
    return 0;
}

// Forward declaration
void db_close(void);

int db_init(void)
{
    static char conninfo[512];
    snprintf(conninfo,
             sizeof(conninfo),
             "host=db_host port=db_port dbname=db_name user=db_user password=db_password");

    db = PQconnectdb(conninfo);
    if (PQstatus(db) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(db));
        PQfinish(db);
        db = NULL;
        return 1;
    }

    printf("Database connection successful.\n");

    if (PQsetnonblocking(db, 1) != 0)
    { // for non-blocking async I/O
        fprintf(stderr, "Failed to set async connection to nonblocking mode\n");
        PQfinish(db);
        db = NULL;
        return 1;
    }

    printf("Async database connection successful.\n");

    if (create_tables() != 0)
    {
        printf("Tables cannot be created\n");
        db_close();
        return 1;
    }

    return 0;
}

void db_close(void)
{
    if (db)
    {
        PQfinish(db);
        db = NULL;
        printf("Database connection closed.\n");
    }
}
```

```c
// handlers.h

#ifndef HANDLERS_H
#define HANDLERS_H

#include "ecewo.h"

// Our example handler
void get_all_users(Req *req, Res *res);

#endif
```

```c
// main.c

#include "handlers.h"
#include "db.h"
#include <stdio.h>

void cleanup_my_app()
{
    db_close();
}

int main(void)
{
    if (server_init() != SERVER_OK)
    {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }

    // Initialize the database
    if (db_init() != 0)
    {
        fprintf(stderr, "Database initialization failed.\n");
        return 1;
    }

    // We access the handler from handlers.h
    get("/all-users", get_all_users);

    shutdown_hook(cleanup_my_app);

    if (server_listen(3000) != SERVER_OK)
    {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }

    server_run();
    return 0;
}
```

Now we can write our first async database operation, which is `get_all_users()`.

## Usage

```c
// get_all_users.c

#include "handlers.h"
#include "cJSON.h"
#include "pquv.h"
#include <stdio.h>

// Callback structure to hold request/response context
typedef struct
{
    Res *res;
} ctx_t;

// Forward declaration
static void users_result_callback(pg_async_t *pg, PGresult *result, void *data);

// Handler
void get_all_users_async(Req *req, Res *res)
{
    const char *sql = "SELECT id, name, username FROM users;";

    // Create context to pass to callback
    ctx_t *ctx = ecewo_alloc(req, sizeof(*ctx)); // use ecewo_alloc, no malloc/calloc
    if (!ctx)
    {
        send_text(res, 500, "Memory allocation failed");
        return;
    }

    // Copy Res to arena
    ctx->res = res;

    // Create async PostgreSQL context
    pg_async_t *pg = pquv_create(db, ctx);
    if (!pg)
    {
        send_text(res, 500, "Failed to create async context");
        return;
    }

    // Queue the query
    int result = pquv_queue(pg, sql, 0, NULL, users_result_callback, ctx);
    if (result != 0)
    {
        send_text(res, 500, "Failed to queue query");
        return;
    }

    // Start execution (this will return immediately)
    result = pquv_execute(pg);
    if (result != 0)
    {
        printf("get_all_users_async: Failed to execute query\n");
        send_text(res, 500, "Failed to execute query");
        return;
    }
}

// Callback function that processes the query result
static void users_result_callback(pg_async_t *pg, PGresult *result, void *data)
{
    ctx_t *ctx = (ctx_t *)data;

    if (!ctx || !ctx->res)
    {
        printf("Invalid context\n");
        return;
    }

    // Check result status
    ExecStatusType status = PQresultStatus(result);
    if (status != PGRES_TUPLES_OK)
    {
        printf("Query failed: %s\n", PQresultErrorMessage(result));
        send_text(ctx->res, 500, "DB select failed");
        return;
    }

    int rows = PQntuples(result);
    cJSON *json_array = cJSON_CreateArray();

    for (int i = 0; i < rows; i++)
    {
        int id = atoi(PQgetvalue(result, i, 0));
        const char *name = PQgetvalue(result, i, 1);
        const char *username = PQgetvalue(result, i, 2);

        cJSON *user_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(user_json, "id", id);
        cJSON_AddStringToObject(user_json, "name", name);
        cJSON_AddStringToObject(user_json, "username", username);

        cJSON_AddItemToArray(json_array, user_json);
    }

    char *json_string = cJSON_PrintUnformatted(json_array);
    send_json(ctx->res, 200, json_string);

    // Cleanup
    cJSON_Delete(json_array);
    free(json_string);

    printf("users_result_callback: Response sent successfully\n");

    // If you want to continue the querying,
    // you should basicaly write a new `pquv_queue()` here
    // it will queue the new query immediately
    // no need to call pquv_execute() again
}
```
