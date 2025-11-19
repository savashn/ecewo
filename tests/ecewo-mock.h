#ifndef MOCK_H
#define MOCK_H

#include <stdint.h>

typedef enum
{
    MOCK_GET,
    MOCK_POST,
    MOCK_PUT,
    MOCK_DELETE,
    MOCK_PATCH
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

void free_request(MockResponse *res);
MockResponse request(MockParams *params);

int mock_setup(void);
void mock_down(void);

void test_routes_hook(test_routes_cb_t callback);

#endif
