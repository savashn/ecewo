#ifndef HANDLERS_H
#define HANDLERS_H
#include <winsock2.h>
#include "../chttp/router.h"

void handle_root(Req *req, Res *res);
void handle_user(Req *req, Res *res);
void handle_post_echo(Req *req, Res *res);

#endif
