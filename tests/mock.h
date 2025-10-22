#ifndef MOCK_H
#define MOCK_H

#include <stdint.h>

typedef enum
{
    GET,
    POST,
    PUT,
    DELETE,
    PATCH
} MockMethod;

typedef struct
{
    uint16_t status_code;
    char *body;
    size_t body_len;
} MockResponse;

typedef struct {
    const char *key;
    const char *value;
} MockHeaders;

typedef struct {
    MockMethod method;
    const char *path;
    const char *body;
    MockHeaders *headers;
    size_t header_count;
} MockParams;

#define TEST_PORT 8888
typedef void (*test_routes_cb_t)(void);

void free_request(MockResponse *resp);
MockResponse request(MockParams *params);

int ecewo_test_setup(void);
int ecewo_test_tear_down(int num_failures);

void test_routes_hook(test_routes_cb_t callback);

#endif
