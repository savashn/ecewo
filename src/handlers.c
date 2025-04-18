#include <stdio.h>
#include "chttp/router.h"
#include "chttp/lib/cjson.h"
#include "chttp/lib/sqlite3.h"

extern sqlite3 *db;

void handle_root(Req *req, Res *res)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "hello", "world");

    char *json_string = cJSON_PrintUnformatted(json);

    reply(res, "200 OK", "application/json", json_string);

    cJSON_Delete(json);
    free(json_string);
}

void get_all_users(Req *req, Res *res)
{
    const char *sql = "SELECT * FROM users;";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        reply(res, "500 Internal Server Error", "text/plain", "DB prepare failed");
        return;
    }

    cJSON *json_array = cJSON_CreateArray();

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        const int id = sqlite3_column_int(stmt, 0);
        const char *username = (const char *)sqlite3_column_text(stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stmt, 2);

        cJSON *user_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(user_json, "id", id);
        cJSON_AddStringToObject(user_json, "username", username);
        cJSON_AddStringToObject(user_json, "name", name);

        cJSON_AddItemToArray(json_array, user_json);
    }

    if (rc != SQLITE_DONE)
    {
        reply(res, "500 Internal Server Error", "text/plain", "DB step failed");
        sqlite3_finalize(stmt);
        return;
    }

    char *json_string = cJSON_PrintUnformatted(json_array);

    reply(res, "200 OK", "application/json", json_string);

    cJSON_Delete(json_array);
    free(json_string);

    sqlite3_finalize(stmt);
}

void add_user(Req *req, Res *res)
{
    const char *body = req->body;
    if (body == NULL)
    {
        reply(res, "400 Bad Request", "text/plain", "Missing request body");
        return;
    }

    cJSON *json = cJSON_Parse(body);
    if (!json)
    {
        reply(res, "400 Bad Request", "text/plain", "Invalid JSON");
        return;
    }

    const char *username = cJSON_GetObjectItem(json, "username")->valuestring;
    const char *name = cJSON_GetObjectItem(json, "name")->valuestring;

    if (!username || !name)
    {
        cJSON_Delete(json);
        reply(res, "400 Bad Request", "text/plain", "Missing fields");
        return;
    }

    const char *sql = "INSERT INTO users (username, name) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        cJSON_Delete(json);
        reply(res, "500 Internal Server Error", "text/plain", "DB prepare failed");
        return;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    cJSON_Delete(json);

    if (rc != SQLITE_DONE)
    {
        reply(res, "500 Internal Server Error", "text/plain", "DB insert failed");
        return;
    }

    reply(res, "201 Created", "application/json", "User created!");
}

void get_user_by_params(Req *req, Res *res)
{
    const char *slug = params_get(&req->params, "slug");

    if (slug == NULL)
    {
        reply(res, "400 Bad Request", "text/plain", "Missing 'id' parameter");
        return;
    }

    const char *sql = "SELECT id, username, name FROM users WHERE username = ?;";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        reply(res, "500 Internal Server Error", "text/plain", "DB prepare failed");
        return;
    }

    sqlite3_bind_text(stmt, 1, slug, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW)
    {
        const int id = sqlite3_column_int(stmt, 0);
        const char *username = (const char *)sqlite3_column_text(stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stmt, 2);

        cJSON *json = cJSON_CreateObject();
        cJSON_AddNumberToObject(json, "id", id);
        cJSON_AddStringToObject(json, "username", username);
        cJSON_AddStringToObject(json, "name", name);

        char *json_string = cJSON_PrintUnformatted(json);

        reply(res, "200 OK", "application/json", json_string);

        cJSON_Delete(json);
        free(json_string);
    }
    else
    {
        reply(res, "404 Not Found", "text/plain", "User not found");
    }

    sqlite3_finalize(stmt);
}

void handle_params(Req *req, Res *res)
{
    printf("req->params pointer: %p\n", (void *)&req->params);

    const char *slug = params_get(&req->params, "slug");

    if (slug == NULL)
    {
        printf("Missing 'slug' parameter\n");
        reply(res, "400 Bad Request", "text/plain", "Missing required parameters: slug");
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
