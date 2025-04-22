#ifndef DB_H
#define DB_H

#include "ecewo/lib/sqlite3.h"

extern sqlite3 *db;

int init_db();

#endif