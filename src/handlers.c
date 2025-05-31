#include "handlers.h"

void hello_world(Req *req, Res *res)
{
  text(200, "hello world!");
}
