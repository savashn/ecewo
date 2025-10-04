# Async Postgres Queries

For asynchronous database queries, Ecewo supports [libpq](https://www.postgresql.org/docs/current/libpq.html) — the official PostgreSQL client library, which includes asynchronous support.

If you are using PostgreSQL, it is recommended to use [ecewo-postgres](https://github.com/savashn/ecewo-packages/tree/main/postgres) for async PostgreSQL queries. It basically combines libpq and libuv for Ecewo.

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

3. Copy the `ecewo-postgres.c` and `ecewo-postgres.h` files from [the repository](https://github.com/savashn/ecewo-packages/tree/main/postgres), paste them into your existing project and make the CMake configuration.

## Configuration

First of all, we need to configure and connect to our database.

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
        "CREATE TABLE IF NOT EXISTS persons ("
        "  id SERIAL PRIMARY KEY, "
        "  name TEXT NOT NULL, "
        "  surname TEXT NOT NULL"
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

void db_close(void)
{
    if (db)
    {
        PQfinish(db);
        db = NULL;
        printf("Database connection closed.\n");
    }
}

int db_init(void)
{
    const char *conninfo = "host=db_host port=db_port dbname=db_name user=db_user password=db_password";

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
```

## Usage

Now let's write an example async query. Here's what we are going to do step by step:

1. We'll get a `name` and `surname` from request query
2. Check if the `name` and `surname` already exists
3. If they don't, we'll insert.

> [!TIP]
>
> In this example, we take the parameters from `req->query` instead of `req->body`. Because it will be simplier to show an example considering using an external library for JSON parsing.

First, create a `handlers.h` file to declare the handler. Because we are going to need this handler in `main.c` file to register it in a route.

```c
// handlers.h

#ifndef HANDLERS_H
#define HANDLERS_H

#include "ecewo.h"

void create_person_handler(Req *req, Res *res);

#endif
```

Now, let's write the handler.

```c
// create_person_handler.c

#include "ecewo.h"
#include "ecewo-postgres.h"
#include "db.h"
#include <stdio.h>

// Callback structure
typedef struct
{
    Res *res;
    const char *name;
    const char *surname;
} ctx_t;

// Forward declarations
static void on_query_person(PGquery *pg, PGresult *result, void *data);
static void on_person_created(PGquery *pg, PGresult *result, void *data);

// Main handler
void create_person_handler(Req *req, Res *res)
{
    const char *name = get_query(req, "name");
    const char *surname = get_query(req, "surname");

    ctx_t *ctx = ecewo_alloc(req, sizeof(ctx_t));
    if (!ctx)
    {
        send_text(res, 500, "Context allocation failed");
        return;
    }

    ctx->res = res;
    ctx->name = name;
    ctx->surname = surname;

    // Create async PostgreSQL context
    PGquery *pg = query_create(res, db, ctx);
    if (!pg)
    {
        send_text(res, 500, "Database connection error");
        return;
    }

    const char *select_sql = "SELECT 1 FROM persons WHERE name = $1 AND surname = $2";
    const char *params[] = {ctx->name, ctx->surname};

    // FIRST QUERY: Check if the person exists
    int query_result = query_queue(pg, select_sql, 2, params, on_query_person, ctx);
    if (query_result != 0)
    {
        printf("ERROR: Failed to queue query, result=%d\n", query_result);
        send_text(res, 500, "Failed to queue query");
        return;
    }

    // Start execution
    int exec_result = query_execute(pg);
    if (exec_result != 0)
    {
        printf("ERROR: Failed to execute, result=%d\n", exec_result);
        send_text(res, 500, "Failed to execute query");
        return;
    }
}

// Callback function that processes the query result
static void on_query_person(PGquery *pg, PGresult *result, void *data)
{
    ctx_t *ctx = (ctx_t *)data;

    if (!result)
    {
        printf("ERROR: Result is NULL\n");
        send_text(ctx->res, 500, "Result not found");
        return;
    }

    ExecStatusType status = PQresultStatus(result);

    if (status != PGRES_TUPLES_OK)
    {
        printf("on_query_person: DB check failed: %s\n", PQresultErrorMessage(result));
        send_text(ctx->res, 500, "Database check failed");
        return;
    }

    if (PQntuples(result) > 0)
    {
        printf("on_query_person: This person already exists\n");
        send_text(ctx->res, 409, "This person already exists");
        return;
    }

    const char *insert_params[2] = {
        ctx->name,
        ctx->surname,
    };

    const char *insert_sql =
        "INSERT INTO persons "
        "(name, surname) "
        "VALUES ($1, $2); ";

    if (query_queue(pg, insert_sql, 2, insert_params, on_person_created, ctx) != 0)
    {
        send_text(ctx->res, 500, "Failed to queue insert query");
        return;
    }

    // No need to call pquv_execute() again
    // query_queue() will run automatically

    printf("on_query_person: Insert operation queued\n");
}

static void on_person_created(PGquery *pg, PGresult *result, void *data)
{
    ctx_t *ctx = (ctx_t *)data;

    ExecStatusType status = PQresultStatus(result);

    if (status != PGRES_COMMAND_OK)
    {
        printf("on_person_created: Person insert failed: %s\n", PQresultErrorMessage(result));
        send_text(ctx->res, 500, "Person insert failed");
        return;
    }

    printf("on_person_created: Person created successfully\n");
    send_text(ctx->res, 201, "Person created successfully");
}
```

## Register The Route and Start The Server
```c
// main.c

#include "handlers.h"
#include "db.h"
#include <stdio.h>

// For shutdown_hook()
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

    // Register the handler
    get("/person", create_person_handler);

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

Let's test it! Send a `GET` request to this URL:

```
http://localhost:3000/person?name=john&surname=doe
```

Check out the `Persons` table in the Postgres and you'll see a person has been added.
