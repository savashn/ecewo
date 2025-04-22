#include <stdio.h>
#include "ecewo/router.h"
#include "ecewo/lib/cjson.h"
#include "ecewo/lib/sqlite3.h"

extern sqlite3 *db;

void hello_world(Req *req, Res *res)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "hello", "world");

    char *json_string = cJSON_PrintUnformatted(json);

    reply(res, "200 OK", "application/json", json_string);

    cJSON_Delete(json);
    free(json_string);
}
