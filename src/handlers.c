#include <stdio.h>
#include "handlers.h"
#include "chttp/router.h"
#include "chttp/lib/cjson.h"

void handle_root(Req *req, Res *res)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "message", "Main Page");

    char *json_string = cJSON_PrintUnformatted(json);

    reply(res, "200 OK", "application/json", json_string);

    cJSON_Delete(json);
    free(json_string);
}

void handle_user(Req *req, Res *res)
{
    cJSON *json = cJSON_CreateObject();

    cJSON *users = cJSON_CreateArray();
    cJSON_AddItemToObject(json, "Users", users);

    cJSON *user1 = cJSON_CreateObject();
    cJSON_AddNumberToObject(user1, "id", 1);
    cJSON_AddStringToObject(user1, "name", "John");
    cJSON_AddStringToObject(user1, "surname", "Doe");

    cJSON *user2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(user2, "id", 2);
    cJSON_AddStringToObject(user2, "name", "Jane");
    cJSON_AddStringToObject(user2, "surname", "Doe");

    cJSON_AddItemToArray(users, user1);
    cJSON_AddItemToArray(users, user2);

    char *json_string = cJSON_PrintUnformatted(json);

    reply(res, "200 OK", "application/json", json_string);

    cJSON_Delete(json);
    free(json_string);
}

void handle_post_echo(Req *req, Res *res)
{
    char json[4096];
    snprintf(json, sizeof(json), "{\"echo\": \"%s\"}", req->body);
    reply(res, "200 OK", "application/json", json);
}
