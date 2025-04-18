#include <stdio.h>
#include "chttp/lib/sqlite3.h"
#include "chttp/server.h"

sqlite3 *db = NULL;

int create_tables()
{
    const char *create_tables =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT NOT NULL,"
        "name TEXT NOT NULL"
        ");"

        "CREATE TABLE IF NOT EXISTS orders ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER NOT NULL,"
        "order_date TEXT NOT NULL,"
        "FOREIGN KEY (user_id) REFERENCES users(id)"
        ");"

        "CREATE TABLE IF NOT EXISTS products ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "product_name TEXT NOT NULL,"
        "price REAL NOT NULL"
        ");";

    char *err_msg = NULL;

    int rc = sqlite3_exec(db, create_tables, 0, 0, &err_msg);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot create the table: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 1;
    }

    printf("Database and tables are ready\n");
    return 0;
}

int init_db()
{
    int rc = sqlite3_open("sql.db", &db);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open the database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    create_tables();

    printf("Database connection successful\n");
    return 0;
}
