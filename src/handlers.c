#include <stdio.h>
#include "handlers.h"
#include "../chttp/utils.h"

void handle_root(Req *req, Res *res)
{
    reply(res, "200 OK", "application/json", "{\"message\": \"Main Page\"}");
}

void handle_user(Req *req, Res *res)
{
    reply(res, "200 OK", "application/json", "{\"id\": 1, \"name\": \"John\", \"surname\": \"Doe\"}");
}

void handle_post_echo(Req *req, Res *res)
{
    char json[4096];
    snprintf(json, sizeof(json), "{\"echo\": \"%s\"}", req->body);
    reply(res, "200 OK", "application/json", json);
}
