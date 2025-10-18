#ifndef MOCK_H
#define MOCK_H

typedef struct
{
    int status_code;
    char *body;
    size_t body_len;
} http_response_t;

#define TEST_PORT 8888

void free_response(http_response_t *resp);
http_response_t http_request(const char *method,
                             const char *path,
                             const char *body, 
                             const char *headers);

#endif
