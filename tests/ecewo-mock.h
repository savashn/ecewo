#ifndef MOCK_H
#define MOCK_H

#include <stdint.h>

typedef enum {
    MOCK_GET,
    MOCK_POST,
    MOCK_PUT,
    MOCK_DELETE,
    MOCK_PATCH
} MockMethod;

typedef struct
{
    const char *key;
    const char *value;
} MockHeaders;

typedef struct
{
    uint16_t status_code;
    char *body;
    size_t body_len;
    MockHeaders *headers;
    size_t header_count;
} MockResponse;

typedef struct
{
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

int mock_init(test_routes_cb_t routes_callback);
void mock_cleanup(void);

const char *mock_get_header(MockResponse *res, const char *key);

#endif
