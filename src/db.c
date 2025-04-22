#include <stdio.h>
#include "ecewo/lib/sqlite3.h"

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
