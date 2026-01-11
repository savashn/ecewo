#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"
#include <string.h>

void handler_single_param(Req *req, Res *res) {
  const char *id = get_param(req, "userId");
  if (!id) {
    send_text(res, 400, "Missing id");
    return;
  }
  char *response = arena_sprintf(req->arena, "id=%s", id);
  send_text(res, 200, response);
}

void handler_multi_param(Req *req, Res *res) {
  const char *userId = get_param(req, "userId");
  const char *postId = get_param(req, "postId");
  const char *commentId = get_param(req, "commentId");

  if (!userId || !postId || !commentId) {
    send_text(res, 400, "Missing params");
    return;
  }

  char *response = arena_sprintf(req->arena, "%s/%s/%s", userId, postId, commentId);
  send_text(res, 200, response);
}

void handler_overflow_param(Req *req, Res *res) {
  const char *id1 = get_param(req, "id1");
  const char *id2 = get_param(req, "id2");
  const char *id3 = get_param(req, "id3");
  const char *id4 = get_param(req, "id4");
  const char *id5 = get_param(req, "id5");
  const char *id6 = get_param(req, "id6");
  const char *id7 = get_param(req, "id7");
  const char *id8 = get_param(req, "id8");
  const char *id9 = get_param(req, "id9");
  const char *id10 = get_param(req, "id10");

  char *response = arena_sprintf(req->arena, "%s/%s/%s/%s/%s/%s/%s/%s/%s/%s",
                                 id1, id2, id3, id4, id5, id6, id7, id8, id9, id10);

  send_text(res, 200, response);
}

int test_single_param(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/users/42"
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_NOT_NULL(res.body);
  ASSERT_EQ_STR("id=42", res.body);

  free_request(&res);
  RETURN_OK();
}

int test_multi_param(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/users/100/posts/200/comments/300"
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_NOT_NULL(res.body);
  ASSERT_EQ_STR("100/200/300", res.body);

  free_request(&res);
  RETURN_OK();
}

int test_param_special_chars(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/users/test-user-123"
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_NOT_NULL(res.body);
  ASSERT_EQ_STR("id=test-user-123", res.body);

  free_request(&res);
  RETURN_OK();
}

int test_overflow_param(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/param/1/2/3/4/5/6/7/8/9/10"
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_NOT_NULL(res.body);
  ASSERT_EQ_STR("1/2/3/4/5/6/7/8/9/10", res.body);

  free_request(&res);
  RETURN_OK();
}

static void setup_routes(void) {
  get("/param/:id1/:id2/:id3/:id4/:id5/:id6/:id7/:id8/:id9/:id10", handler_overflow_param);
  get("/users/:userId/posts/:postId/comments/:commentId", handler_multi_param);
  get("/users/:userId", handler_single_param);
}

int main(void) {
  mock_init(setup_routes);
  RUN_TEST(test_single_param);
  RUN_TEST(test_multi_param);
  RUN_TEST(test_param_special_chars);
  RUN_TEST(test_overflow_param);
  mock_cleanup();
  return 0;
}
