#include <stdio.h>
#include "handlers.h"
#include "chttp/router.h"
#include "chttp/lib/cjson.h"
#include "chttp/lib/sqlite3.h"

extern sqlite3 *db;

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

void handle_create_user(Req *req, Res *res)
{
    cJSON *json = cJSON_Parse(req->body);

    if (!json)
    {
        reply(res, "400 Bad Request", "application/json", "{\"error\": \"Invalid JSON\"}");
        return;
    }

    cJSON *username_item = cJSON_GetObjectItemCaseSensitive(json, "username");

    if (cJSON_IsString(username_item) || username_item->valuestring == NULL)
    {
        cJSON_Delete(json);
        reply(res, "400 Bad Request", "application/json", "{\"error\": \"Missing or invalid 'username' field\"}");
        return;
    }

    const char *username = username_item->valuestring;

    const char *sql = "INSERT INTO users (username) VALUES (?)";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        cJSON_Delete(json);
        reply(res, "500 Internal Server Error", "application/json", "{\"error\": \"Failed to prepare statement\"}");
        return;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        cJSON_Delete(json);
        reply(res, "500 Internal Server Error", "application/json", "{\"error\": \"Failed to insert user\"}");
        return;
    }

    sqlite3_finalize(stmt);
    cJSON_Delete(json);

    char response[256];
    snprintf(response, sizeof(response),
             "{\"message\": \"User '%s' added successfully\"}",
             username);
    reply(res, "201 Created", "application/json", response);
}

void handle_params(Req *req, Res *res)
{
    printf("req->params pointer: %p\n", (void *)&req->params);

    const char *slug = params_get(&req->params, "slug");

    if (slug == NULL)
    {
        reply(res, "400 Bad Request", "text/plain", "Missing required parameters: slug or id");
        return;
    }

    printf("Slug: %s\n", slug);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "slug", slug);
    char *json_string = cJSON_PrintUnformatted(json);

    reply(res, "200 OK", "application/json", json_string);

    cJSON_Delete(json);
    free(json_string);
}

void handle_query(Req *req, Res *res)
{
    printf("req->query pointer: %p\n", (void *)&req->query);

    const char *name = query_get(&req->query, "name");
    const char *surname = query_get(&req->query, "surname");

    if (name == NULL || surname == NULL)
    {
        reply(res, "400 Bad Request", "text/plain", "Missing required parameters: name or surname");
        return;
    }

    printf("Name: %s\n", name);
    printf("Surname: %s\n", surname);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "name", name);
    cJSON_AddStringToObject(json, "surname", surname);
    char *json_string = cJSON_PrintUnformatted(json);

    reply(res, "200 OK", "application/json", json_string);

    cJSON_Delete(json);
    free(json_string);
}

void handle_params_and_query(Req *req, Res *res)
{
    const char *slug = params_get(&req->params, "slug");
    const char *id = params_get(&req->params, "id");
    const char *name = query_get(&req->query, "name");
    const char *surname = query_get(&req->query, "surname");

    if (slug == NULL || id == NULL)
    {
        reply(res, "400 Bad Request", "text/plain", "Missing required parameters: slug or id");
        return;
    }

    printf("Slug: %s\n", slug);
    printf("ID: %s\n", id);
    printf("Name: %s\n", name);
    printf("Surname: %s\n", surname);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "slug", slug);
    cJSON_AddStringToObject(json, "id", id);
    cJSON_AddStringToObject(json, "name", name);
    cJSON_AddStringToObject(json, "surname", surname);
    char *json_string = cJSON_PrintUnformatted(json);

    reply(res, "200 OK", "application/json", json_string);

    cJSON_Delete(json);
    free(json_string);
}
