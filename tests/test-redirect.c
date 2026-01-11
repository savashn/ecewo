#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"

void handler_redirect(Req *req, Res *res) {
  (void)req;
  redirect(res, MOVED_PERMANENTLY, "/new-location");
}

void handler_new_location(Req *req, Res *res) {
  (void)req;
  send_text(res, OK, "New page content");
}

int test_redirect(void) {
  MockParams params1 = {
    .method = MOCK_GET,
    .path = "/old-path"
  };

  MockResponse res1 = request(&params1);

  ASSERT_EQ(301, res1.status_code);

  const char *location = mock_get_header(&res1, "Location");
  ASSERT_NOT_NULL(location);
  ASSERT_EQ_STR("/new-location", location);

  MockParams params2 = {
    .method = MOCK_GET,
    .path = location
  };

  MockResponse res2 = request(&params2);

  ASSERT_EQ(200, res2.status_code);
  ASSERT_EQ_STR("New page content", res2.body);

  free_request(&res1);
  free_request(&res2);
  RETURN_OK();
}

void handler_redirect_injection(Req *req, Res *res) {
  (void)req;

  const char *evil_url = "https://motherfuckingmaliciouswebsite.com\r\nSet-Cookie: session=stolen";
  redirect(res, 302, evil_url);
}

int test_redirect_injection(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/redirect-injection"
  };

  MockResponse res = request(&params);

  ASSERT_EQ(400, res.status_code);
  ASSERT_EQ_STR("Bad Request", res.body);

  ASSERT_NULL(mock_get_header(&res, "Location"));
  ASSERT_NULL(mock_get_header(&res, "Set-Cookie"));

  free_request(&res);
  RETURN_OK();
}

static void setup_routes(void) {
  get("/old-path", handler_redirect);
  get("/new-location", handler_new_location);
  get("/redirect-injection", handler_redirect_injection);
}

int main(void) {
  mock_init(setup_routes);
  RUN_TEST(test_redirect);
  RUN_TEST(test_redirect_injection);
  mock_cleanup();
  return 0;
}
