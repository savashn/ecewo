#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"
#include <string.h>

void handler_method_echo(Req *req, Res *res) {
  send_text(res, 200, req->method);
}

void handler_post_body(Req *req, Res *res) {
  if (req->body_len == 0) {
    send_text(res, 400, "No body");
    return;
  }

  char *response = arena_sprintf(req->arena,
                                 "len=%zu,body=%s",
                                 req->body_len,
                                 req->body);

  send_text(res, 200, response);
}

int test_method_get(void) {
  MockParams params = { .method = MOCK_GET, .path = "/method" };
  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_EQ_STR("GET", res.body);

  free_request(&res);
  RETURN_OK();
}

int test_method_post(void) {
  MockParams params = { .method = MOCK_POST, .path = "/method" };
  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_EQ_STR("POST", res.body);

  free_request(&res);
  RETURN_OK();
}

int test_method_put(void) {
  MockParams params = { .method = MOCK_PUT, .path = "/method" };
  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_EQ_STR("PUT", res.body);

  free_request(&res);
  RETURN_OK();
}

int test_method_delete(void) {
  MockParams params = { .method = MOCK_DELETE, .path = "/method" };
  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_EQ_STR("DELETE", res.body);

  free_request(&res);
  RETURN_OK();
}

int test_method_patch(void) {
  MockParams params = { .method = MOCK_PATCH, .path = "/method" };
  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_EQ_STR("PATCH", res.body);

  free_request(&res);
  RETURN_OK();
}

int test_post_with_body(void) {
  MockParams params = {
    .method = MOCK_POST,
    .path = "/echo-body",
    .body = "{\"test\":true}"
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_EQ_STR("len=13,body={\"test\":true}", res.body);

  free_request(&res);
  RETURN_OK();
}

static void setup_routes(void) {
  get("/method", handler_method_echo);
  post("/method", handler_method_echo);
  put("/method", handler_method_echo);
  del("/method", handler_method_echo);
  patch("/method", handler_method_echo);
  post("/echo-body", handler_post_body);
}

int main(void) {
  mock_init(setup_routes);
  RUN_TEST(test_method_get);
  RUN_TEST(test_method_post);
  RUN_TEST(test_method_put);
  RUN_TEST(test_method_delete);
  RUN_TEST(test_method_patch);
  RUN_TEST(test_post_with_body);
  mock_cleanup();
  return 0;
}
