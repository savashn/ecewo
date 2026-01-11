#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"

static int middleware_order_tracker = 0;

void middleware_first(Req *req, Res *res, Next next) {
  int *order = arena_alloc(req->arena, sizeof(int));
  *order = ++middleware_order_tracker;
  set_context(req, "first", order);
  next(req, res);
}

void middleware_second(Req *req, Res *res, Next next) {
  int *order = arena_alloc(req->arena, sizeof(int));
  *order = ++middleware_order_tracker;
  set_context(req, "second", order);
  next(req, res);
}

void middleware_third(Req *req, Res *res, Next next) {
  int *order = arena_alloc(req->arena, sizeof(int));
  *order = ++middleware_order_tracker;
  set_context(req, "third", order);
  next(req, res);
}

void handler_middleware_order(Req *req, Res *res) {
  int *first = get_context(req, "first");
  int *second = get_context(req, "second");
  int *third = get_context(req, "third");

  char *response = arena_sprintf(req->arena, "%d,%d,%d",
                                 first ? *first : 0,
                                 second ? *second : 0,
                                 third ? *third : 0);

  send_text(res, 200, response);
}

void middleware_abort(Req *req, Res *res, Next next) {
  (void)req;
  (void)next;
  send_text(res, 403, "Forbidden by middleware");
  return; // Don't call next
}

void handler_should_not_reach(Req *req, Res *res) {
  (void)req;
  send_text(res, 200, "Should not see this");
}

int test_middleware_execution_order(void) {
  middleware_order_tracker = 0;

  MockParams params = {
    .method = MOCK_GET,
    .path = "/mw-order"
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_EQ_STR("1,2,3", res.body);

  free_request(&res);
  RETURN_OK();
}

int test_middleware_abort(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/mw-abort"
  };

  MockResponse res = request(&params);

  ASSERT_EQ(403, res.status_code);
  ASSERT_EQ_STR("Forbidden by middleware", res.body);

  free_request(&res);
  RETURN_OK();
}

static void setup_routes(void) {
  get("/mw-order", middleware_first, middleware_second, middleware_third, handler_middleware_order);
  get("/mw-abort", middleware_abort, handler_should_not_reach);
}

int main(void) {
  mock_init(setup_routes);

  RUN_TEST(test_middleware_execution_order);
  RUN_TEST(test_middleware_abort);

  mock_cleanup();
  return 0;
}