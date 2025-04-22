#include <stdio.h>
#include "ecewo/server.h"
#include "db.h"

int main()
{
    init_db();
    ecewo();
    sqlite3_close(db);
    return 0;
}