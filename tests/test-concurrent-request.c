#include "ecewo.h"
#include "ecewo-mock.h"
#include "tester.h"

void handler_counter(Req *req, Res *res)
{
    static int counter = 0;
    counter++;
    char *response = arena_sprintf(req->arena, "%d", counter);
    send_text(res, 200, response);
}

int test_sequential_requests(void)
{
    // 10 sequential request
    for (int i = 1; i <= 10; i++) {
        MockParams params = {
            .method = MOCK_GET,
            .path = "/counter"
        };

        MockResponse res = request(&params);
        ASSERT_EQ(200, res.status_code);

        int count = atoi(res.body);
        ASSERT_EQ(i, count);

        free_request(&res);
    }

    RETURN_OK();
}
