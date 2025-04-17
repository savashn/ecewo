#include <stdio.h>
#include "chttp/lib/sqlite3.h"
#include "chttp/server.h"

sqlite3 *db = NULL;

int init_db()
{
    int rc = sqlite3_open("sql.db", &db);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open the database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    printf("Database connection successful\n");
    return 0;
}

int users_table()
{
    int rc = sqlite3_open("sql.db", &db);
    if (rc)
    {
        fprintf(stderr, "Cannot open the database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    const char *create_table_sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT NOT NULL"
        ");";

    char *err_msg = NULL;

    rc = sqlite3_exec(db, create_table_sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot create the table: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 1;
    }

    printf("Database and tables are ready\n");
    return 0;
}
