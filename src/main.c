#include <stdio.h>
#include "chttp/server.h"
#include "db.h"

int main()
{
    init_db();
    run();
    sqlite3_close(db);
    return 0;
}