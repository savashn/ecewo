#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"
#include <string.h>

void handler_query_params(Req *req, Res *res) {
  const char *page = get_query(req, "page");
  const char *limit = get_query(req, "limit");
  const char *sort = get_query(req, "sort");

  char *response = arena_sprintf(req->arena, "page=%s,limit=%s,sort=%s",
                                 page ? page : "null",
                                 limit ? limit : "null",
                                 sort ? sort : "null");

  send_text(res, 200, response);
}

int test_query_multiple(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/search?page=1&limit=10&sort=desc"
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_EQ_STR("page=1,limit=10,sort=desc", res.body);

  free_request(&res);
  RETURN_OK();
}

int test_query_empty_value(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/search?page=1&limit=&sort=asc"
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  // Empty value should result in null (based on parse_query logic)
  ASSERT_EQ_STR("page=1,limit=null,sort=asc", res.body);

  free_request(&res);
  RETURN_OK();
}

int test_query_no_params(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/search"
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_EQ_STR("page=null,limit=null,sort=null", res.body);

  free_request(&res);
  RETURN_OK();
}

static void setup_routes(void) {
  get("/search", handler_query_params);
}

int main(void) {
  mock_init(setup_routes);

  RUN_TEST(test_query_multiple);
  RUN_TEST(test_query_empty_value);
  RUN_TEST(test_query_no_params);

  mock_cleanup();
  return 0;
}
