#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"

void handler_redirect(Req *req, Res *res)
{
    (void)req;
    redirect(res, MOVED_PERMANENTLY, "/new-location");
}

int test_redirect_301(void)
{
    MockParams params = {
        .method = MOCK_GET,
        .path = "/old-path"
    };
    
    MockResponse res = request(&params);
    ASSERT_EQ(301, res.status_code);
    
    free_request(&res);
    RETURN_OK();
}
