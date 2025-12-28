#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"

void handler_root(Req *req, Res *res)
{
    (void)req;
    send_text(res, 200, "root");
}

int test_root_path(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/"
    };

    MockResponse res = request(&params);

    ASSERT_EQ(200, res.status_code);
    ASSERT_EQ_STR("root", res.body);

    free_request(&res);
    RETURN_OK();
}
