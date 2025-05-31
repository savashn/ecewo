#include "server.h"
#include "handlers.h"

int main()
{
  init_router();
  get("/", hello_world);
  ecewo(4000);
  final_router();
  return 0;
}
