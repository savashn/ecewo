#ifndef HANDLERS_H
#define HANDLERS_H
#include "chttp/router.h"

void handle_root(Req *req, Res *res);
void get_all_users(Req *req, Res *res);
void add_user(Req *req, Res *res);
void get_user_by_params(Req *req, Res *res);
void handle_params(Req *req, Res *res);
void handle_query(Req *req, Res *res);
void handle_params_and_query(Req *req, Res *res);

#endif
