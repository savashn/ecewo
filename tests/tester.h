// MIT License

// Copyright (c) 2025 savashn

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef MYASSERT_H
#define MYASSERT_H

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define FATAL(msg)                                        \
  do {                                                    \
    fprintf(stderr,                                       \
            "Fatal error in %s on line %d: %s\n",         \
            __FILE__,                                     \
            __LINE__,                                     \
            msg);                                         \
    fflush(stderr);                                       \
    abort();                                              \
  } while (0)

#define ASSERT(expr)                                      \
 do {                                                     \
  if (!(expr)) {                                          \
    fprintf(stderr,                                       \
            "Assertion failed in %s on line %d: %s\n",    \
            __FILE__,                                     \
            __LINE__,                                     \
            #expr);                                       \
    abort();                                              \
  }                                                       \
 } while (0)

#define ASSERT_BASE(a, operator, b, type, conv)              \
 do {                                                        \
  type const eval_a = (a);                                   \
  type const eval_b = (b);                                   \
  if (!(eval_a operator eval_b)) {                           \
    fprintf(stderr,                                          \
            "Assertion failed in %s on line %d: `%s %s %s` " \
            "(%"conv" %s %"conv")\n",                        \
            __FILE__,                                        \
            __LINE__,                                        \
            #a,                                              \
            #operator,                                       \
            #b,                                              \
            eval_a,                                          \
            #operator,                                       \
            eval_b);                                         \
    abort();                                                 \
  }                                                          \
 } while (0)

#define ASSERT_OK(a)                                         \
 do {                                                        \
  int64_t const eval_a = (a);                                \
  if (eval_a) {                                              \
    fprintf(stderr,                                          \
            "Assertion failed in %s on line %d: `%s` okay "  \
            "(error: %"PRId64")\n",                          \
            __FILE__,                                        \
            __LINE__,                                        \
            #a,                                              \
            eval_a);                                         \
    abort();                                                 \
  }                                                          \
 } while (0)

#define ASSERT_BASE_STR(expr, a, operator, b, type, conv)      \
 do {                                                          \
  if (!(expr)) {                                               \
    fprintf(stderr,                                            \
            "Assertion failed in %s on line %d: `%s %s %s` "   \
            "(%"conv" %s %"conv")\n",                          \
            __FILE__,                                          \
            __LINE__,                                          \
            #a,                                                \
            #operator,                                         \
            #b,                                                \
            (type)a,                                           \
            #operator,                                         \
            (type)b);                                          \
    abort();                                                   \
  }                                                            \
 } while (0)

#define ASSERT_BASE_LEN(expr, a, operator, b, conv, len)     \
 do {                                                        \
  if (!(expr)) {                                             \
    fprintf(stderr,                                          \
            "Assertion failed in %s on line %d: `%s %s %s` " \
            "(%.*"#conv" %s %.*"#conv")\n",                  \
            __FILE__,                                        \
            __LINE__,                                        \
            #a,                                              \
            #operator,                                       \
            #b,                                              \
            (int)len,                                        \
            a,                                               \
            #operator,                                       \
            (int)len,                                        \
            b);                                              \
    abort();                                                 \
  }                                                          \
 } while (0)

enum TestStatus {
  TEST_OK = 0,
  TEST_SKIP = 1
};

#define RETURN_OK()                                                           \
  do {                                                                        \
    return TEST_OK;                                                           \
  } while (0)

#define RETURN_SKIP(explanation)                                              \
  do {                                                                        \
    fprintf(stderr, "%s\n", explanation);                                     \
    fflush(stderr);                                                           \
    return TEST_SKIP;                                                         \
  } while (0)

#define RUN_TEST(test_func)                                                   \
  do {                                                                        \
    printf("Running %s... ", #test_func);                                     \
    fflush(stdout);                                                           \
    int result = test_func();                                                 \
    if (result == TEST_OK) {                                                  \
      printf("PASSED\n");                                                     \
    } else if (result == TEST_SKIP) {                                         \
      printf("SKIPPED\n");                                                    \
    } else {                                                                  \
      printf("FAILED\n");                                                     \
    }                                                                         \
  } while (0)


// =============================================================
// BOOLEAN ASSERTIONS
// =============================================================

#define ASSERT_TRUE(a) \
  ASSERT_BASE(a, ==, true, bool, "d")

#define ASSERT_FALSE(a) \
  ASSERT_BASE(a, ==, false, bool, "d")


// =============================================================
// NULL ASSERTIONS
// =============================================================

#define ASSERT_NULL(a) \
  ASSERT_BASE(a, ==, NULL, const void*, "p")

#define ASSERT_NOT_NULL(a) \
  ASSERT_BASE(a, !=, NULL, const void*, "p")


// =============================================================
// MEMORY ASSERTIONS
// =============================================================

#define ASSERT_EQ_MEM(a, b, len) \
  ASSERT_BASE_LEN(memcmp(a, b, len) == 0, a, ==, b, "p", len)

#define ASSERT_NE_MEM(a, b, len) \
  ASSERT_BASE_LEN(memcmp(a, b, len) != 0, a, !=, b, "p", len)


// =============================================================
// STRING ASSERTIONS
// =============================================================

#define ASSERT_EQ_STR(a, b) \
  ASSERT_BASE_STR(strcmp(a, b) == 0, a, == , b, char*, "s")

#define ASSERT_NE_STR(a, b) \
  ASSERT_BASE_STR(strcmp(a, b) != 0, a, !=, b, char*, "s")

#define ASSERT_EQ_STR_LEN(a, b, len) \
  ASSERT_BASE_LEN(strncmp(a, b, len) == 0, a, ==, b, "s", len)

#define ASSERT_NE_STR_LEN(a, b, len) \
  ASSERT_BASE_LEN(strncmp(a, b, len) != 0, a, !=, b, "s", len)


// ==============================================
// BASIC INTEGER ASSERTIONS
// ==============================================

#define ASSERT_EQ(a, b) ASSERT_BASE(a, ==, b, int64_t, PRId64)
#define ASSERT_GE(a, b) ASSERT_BASE(a, >=, b, int64_t, PRId64)
#define ASSERT_GT(a, b) ASSERT_BASE(a, >, b, int64_t, PRId64)
#define ASSERT_LE(a, b) ASSERT_BASE(a, <=, b, int64_t, PRId64)
#define ASSERT_LT(a, b) ASSERT_BASE(a, <, b, int64_t, PRId64)
#define ASSERT_NE(a, b) ASSERT_BASE(a, !=, b, int64_t, PRId64)


// ==============================================
// TYPE-SAFE INTEGER ASSERTIONS
// ==============================================

#define TYPE_CHECK(expr, expected_type) do { \
    expected_type _type_check_var = expr; \
    (void)_type_check_var; \
    (void)sizeof(char[sizeof(expr) == sizeof(expected_type) ? 1 : -1]); \
} while(0)

// ==============================================
// INT8_T ASSERTIONS
// ==============================================

#define ASSERT_EQ_INT8(a, b) \
do { \
    TYPE_CHECK(a, int8_t); \
    TYPE_CHECK(b, int8_t); \
    ASSERT_BASE(a, ==, b, int8_t, PRId8); \
} while(0)

#define ASSERT_GE_INT8(a, b) \
do { \
    TYPE_CHECK(a, int8_t); \
    TYPE_CHECK(b, int8_t); \
    ASSERT_BASE(a, >=, b, int8_t, PRId8); \
} while(0)

#define ASSERT_GT_INT8(a, b) \
do { \
    TYPE_CHECK(a, int8_t); \
    TYPE_CHECK(b, int8_t); \
    ASSERT_BASE(a, >, b, int8_t, PRId8); \
} while(0)

#define ASSERT_LE_INT8(a, b) \
do { \
    TYPE_CHECK(a, int8_t); \
    TYPE_CHECK(b, int8_t); \
    ASSERT_BASE(a, <=, b, int8_t, PRId8); \
} while(0)

#define ASSERT_LT_INT8(a, b) \
do { \
    TYPE_CHECK(a, int8_t); \
    TYPE_CHECK(b, int8_t); \
    ASSERT_BASE(a, <, b, int8_t, PRId8); \
} while(0)

#define ASSERT_NE_INT8(a, b) \
do { \
    TYPE_CHECK(a, int8_t); \
    TYPE_CHECK(b, int8_t); \
    ASSERT_BASE(a, !=, b, int8_t, PRId8); \
} while(0)


// ==============================================
// UINT8_T ASSERTIONS
// ==============================================

#define ASSERT_EQ_UINT8(a, b) \
do { \
    TYPE_CHECK(a, uint8_t); \
    TYPE_CHECK(b, uint8_t); \
    ASSERT_BASE(a, ==, b, uint8_t, PRIu8); \
} while(0)

#define ASSERT_GE_UINT8(a, b) \
do { \
    TYPE_CHECK(a, uint8_t); \
    TYPE_CHECK(b, uint8_t); \
    ASSERT_BASE(a, >=, b, uint8_t, PRIu8); \
} while(0)

#define ASSERT_GT_UINT8(a, b) \
do { \
    TYPE_CHECK(a, uint8_t); \
    TYPE_CHECK(b, uint8_t); \
    ASSERT_BASE(a, >, b, uint8_t, PRIu8); \
} while(0)

#define ASSERT_LE_UINT8(a, b) \
do { \
    TYPE_CHECK(a, uint8_t); \
    TYPE_CHECK(b, uint8_t); \
    ASSERT_BASE(a, <=, b, uint8_t, PRIu8); \
} while(0)

#define ASSERT_LT_UINT8(a, b) \
do { \
    TYPE_CHECK(a, uint8_t); \
    TYPE_CHECK(b, uint8_t); \
    ASSERT_BASE(a, <, b, uint8_t, PRIu8); \
} while(0)

#define ASSERT_NE_UINT8(a, b) \
do { \
    TYPE_CHECK(a, uint8_t); \
    TYPE_CHECK(b, uint8_t); \
    ASSERT_BASE(a, !=, b, uint8_t, PRIu8); \
} while(0)


// ==============================================
// INT16_T ASSERTIONS
// ==============================================

#define ASSERT_EQ_INT16(a, b) \
do { \
    TYPE_CHECK(a, int16_t); \
    TYPE_CHECK(b, int16_t); \
    ASSERT_BASE(a, ==, b, int16_t, PRId16); \
} while(0)

#define ASSERT_GE_INT16(a, b) \
do { \
    TYPE_CHECK(a, int16_t); \
    TYPE_CHECK(b, int16_t); \
    ASSERT_BASE(a, >=, b, int16_t, PRId16); \
} while(0)

#define ASSERT_GT_INT16(a, b) \
do { \
    TYPE_CHECK(a, int16_t); \
    TYPE_CHECK(b, int16_t); \
    ASSERT_BASE(a, >, b, int16_t, PRId16); \
} while(0)

#define ASSERT_LE_INT16(a, b) \
do { \
    TYPE_CHECK(a, int16_t); \
    TYPE_CHECK(b, int16_t); \
    ASSERT_BASE(a, <=, b, int16_t, PRId16); \
} while(0)

#define ASSERT_LT_INT16(a, b) \
do { \
    TYPE_CHECK(a, int16_t); \
    TYPE_CHECK(b, int16_t); \
    ASSERT_BASE(a, <, b, int16_t, PRId16); \
} while(0)

#define ASSERT_NE_INT16(a, b) \
do { \
    TYPE_CHECK(a, int16_t); \
    TYPE_CHECK(b, int16_t); \
    ASSERT_BASE(a, !=, b, int16_t, PRId16); \
} while(0)


// ==============================================
// UINT16_T ASSERTIONS
// ==============================================

#define ASSERT_EQ_UINT16(a, b) \
do { \
    TYPE_CHECK(a, uint16_t); \
    TYPE_CHECK(b, uint16_t); \
    ASSERT_BASE(a, ==, b, uint16_t, PRIu16); \
} while(0)

#define ASSERT_GE_UINT16(a, b) \
do { \
    TYPE_CHECK(a, uint16_t); \
    TYPE_CHECK(b, uint16_t); \
    ASSERT_BASE(a, >=, b, uint16_t, PRIu16); \
} while(0)

#define ASSERT_GT_UINT16(a, b) \
do { \
    TYPE_CHECK(a, uint16_t); \
    TYPE_CHECK(b, uint16_t); \
    ASSERT_BASE(a, >, b, uint16_t, PRIu16); \
} while(0)

#define ASSERT_LE_UINT16(a, b) \
do { \
    TYPE_CHECK(a, uint16_t); \
    TYPE_CHECK(b, uint16_t); \
    ASSERT_BASE(a, <=, b, uint16_t, PRIu16); \
} while(0)

#define ASSERT_LT_UINT16(a, b) \
do { \
    TYPE_CHECK(a, uint16_t); \
    TYPE_CHECK(b, uint16_t); \
    ASSERT_BASE(a, <, b, uint16_t, PRIu16); \
} while(0)

#define ASSERT_NE_UINT16(a, b) \
do { \
    TYPE_CHECK(a, uint16_t); \
    TYPE_CHECK(b, uint16_t); \
    ASSERT_BASE(a, !=, b, uint16_t, PRIu16); \
} while(0)


// ==============================================
// INT32_T ASSERTIONS
// ==============================================

#define ASSERT_EQ_INT32(a, b) \
do { \
    TYPE_CHECK(a, int32_t); \
    TYPE_CHECK(b, int32_t); \
    ASSERT_BASE(a, ==, b, int32_t, PRId32); \
} while(0)

#define ASSERT_GE_INT32(a, b) \
do { \
    TYPE_CHECK(a, int32_t); \
    TYPE_CHECK(b, int32_t); \
    ASSERT_BASE(a, >=, b, int32_t, PRId32); \
} while(0)

#define ASSERT_GT_INT32(a, b) \
do { \
    TYPE_CHECK(a, int32_t); \
    TYPE_CHECK(b, int32_t); \
    ASSERT_BASE(a, >, b, int32_t, PRId32); \
} while(0)

#define ASSERT_LE_INT32(a, b) \
do { \
    TYPE_CHECK(a, int32_t); \
    TYPE_CHECK(b, int32_t); \
    ASSERT_BASE(a, <=, b, int32_t, PRId32); \
} while(0)

#define ASSERT_LT_INT32(a, b) \
do { \
    TYPE_CHECK(a, int32_t); \
    TYPE_CHECK(b, int32_t); \
    ASSERT_BASE(a, <, b, int32_t, PRId32); \
} while(0)

#define ASSERT_NE_INT32(a, b) \
do { \
    TYPE_CHECK(a, int32_t); \
    TYPE_CHECK(b, int32_t); \
    ASSERT_BASE(a, !=, b, int32_t, PRId32); \
} while(0)


// ==============================================
// UINT32_T ASSERTIONS
// ==============================================

#define ASSERT_EQ_UINT32(a, b) \
do { \
    TYPE_CHECK(a, uint32_t); \
    TYPE_CHECK(b, uint32_t); \
    ASSERT_BASE(a, ==, b, uint32_t, PRIu32); \
} while(0)

#define ASSERT_GE_UINT32(a, b) \
do { \
    TYPE_CHECK(a, uint32_t); \
    TYPE_CHECK(b, uint32_t); \
    ASSERT_BASE(a, >=, b, uint32_t, PRIu32); \
} while(0)

#define ASSERT_GT_UINT32(a, b) \
do { \
    TYPE_CHECK(a, uint32_t); \
    TYPE_CHECK(b, uint32_t); \
    ASSERT_BASE(a, >, b, uint32_t, PRIu32); \
} while(0)

#define ASSERT_LE_UINT32(a, b) \
do { \
    TYPE_CHECK(a, uint32_t); \
    TYPE_CHECK(b, uint32_t); \
    ASSERT_BASE(a, <=, b, uint32_t, PRIu32); \
} while(0)

#define ASSERT_LT_UINT32(a, b) \
do { \
    TYPE_CHECK(a, uint32_t); \
    TYPE_CHECK(b, uint32_t); \
    ASSERT_BASE(a, <, b, uint32_t, PRIu32); \
} while(0)

#define ASSERT_NE_UINT32(a, b) \
do { \
    TYPE_CHECK(a, uint32_t); \
    TYPE_CHECK(b, uint32_t); \
    ASSERT_BASE(a, !=, b, uint32_t, PRIu32); \
} while(0)


// ==============================================
// INT64_T ASSERTIONS
// ==============================================

#define ASSERT_EQ_INT64(a, b) \
do { \
    TYPE_CHECK(a, int64_t); \
    TYPE_CHECK(b, int64_t); \
    ASSERT_BASE(a, ==, b, int64_t, PRId64); \
} while(0)

#define ASSERT_GE_INT64(a, b) \
do { \
    TYPE_CHECK(a, int64_t); \
    TYPE_CHECK(b, int64_t); \
    ASSERT_BASE(a, >=, b, int64_t, PRId64); \
} while(0)

#define ASSERT_GT_INT64(a, b) \
do { \
    TYPE_CHECK(a, int64_t); \
    TYPE_CHECK(b, int64_t); \
    ASSERT_BASE(a, >, b, int64_t, PRId64); \
} while(0)

#define ASSERT_LE_INT64(a, b) \
do { \
    TYPE_CHECK(a, int64_t); \
    TYPE_CHECK(b, int64_t); \
    ASSERT_BASE(a, <=, b, int64_t, PRId64); \
} while(0)

#define ASSERT_LT_INT64(a, b) \
do { \
    TYPE_CHECK(a, int64_t); \
    TYPE_CHECK(b, int64_t); \
    ASSERT_BASE(a, <, b, int64_t, PRId64); \
} while(0)

#define ASSERT_NE_INT64(a, b) \
do { \
    TYPE_CHECK(a, int64_t); \
    TYPE_CHECK(b, int64_t); \
    ASSERT_BASE(a, !=, b, int64_t, PRId64); \
} while(0)


// ==============================================
// UINT64_T ASSERTIONS
// ==============================================

#define ASSERT_EQ_UINT64(a, b) \
do { \
    TYPE_CHECK(a, uint64_t); \
    TYPE_CHECK(b, uint64_t); \
    ASSERT_BASE(a, ==, b, uint64_t, PRIu64); \
} while(0)

#define ASSERT_GE_UINT64(a, b) \
do { \
    TYPE_CHECK(a, uint64_t); \
    TYPE_CHECK(b, uint64_t); \
    ASSERT_BASE(a, >=, b, uint64_t, PRIu64); \
} while(0)

#define ASSERT_GT_UINT64(a, b) \
do { \
    TYPE_CHECK(a, uint64_t); \
    TYPE_CHECK(b, uint64_t); \
    ASSERT_BASE(a, >, b, uint64_t, PRIu64); \
} while(0)

#define ASSERT_LE_UINT64(a, b) \
do { \
    TYPE_CHECK(a, uint64_t); \
    TYPE_CHECK(b, uint64_t); \
    ASSERT_BASE(a, <=, b, uint64_t, PRIu64); \
} while(0)

#define ASSERT_LT_UINT64(a, b) \
do { \
    TYPE_CHECK(a, uint64_t); \
    TYPE_CHECK(b, uint64_t); \
    ASSERT_BASE(a, <, b, uint64_t, PRIu64); \
} while(0)

#define ASSERT_NE_UINT64(a, b) \
do { \
    TYPE_CHECK(a, uint64_t); \
    TYPE_CHECK(b, uint64_t); \
    ASSERT_BASE(a, !=, b, uint64_t, PRIu64); \
} while(0)


// ==============================================
// SIZE_T ASSERTIONS
// ==============================================

#define ASSERT_EQ_SIZE(a, b) \
do { \
    TYPE_CHECK(a, size_t); \
    TYPE_CHECK(b, size_t); \
    ASSERT_BASE(a, ==, b, size_t, "zu"); \
} while(0)

#define ASSERT_GE_SIZE(a, b) \
do { \
    TYPE_CHECK(a, size_t); \
    TYPE_CHECK(b, size_t); \
    ASSERT_BASE(a, >=, b, size_t, "zu"); \
} while(0)

#define ASSERT_GT_SIZE(a, b) \
do { \
    TYPE_CHECK(a, size_t); \
    TYPE_CHECK(b, size_t); \
    ASSERT_BASE(a, >, b, size_t, "zu"); \
} while(0)

#define ASSERT_LE_SIZE(a, b) \
do { \
    TYPE_CHECK(a, size_t); \
    TYPE_CHECK(b, size_t); \
    ASSERT_BASE(a, <=, b, size_t, "zu"); \
} while(0)

#define ASSERT_LT_SIZE(a, b) \
do { \
    TYPE_CHECK(a, size_t); \
    TYPE_CHECK(b, size_t); \
    ASSERT_BASE(a, <, b, size_t, "zu"); \
} while(0)

#define ASSERT_NE_SIZE(a, b) \
do { \
    TYPE_CHECK(a, size_t); \
    TYPE_CHECK(b, size_t); \
    ASSERT_BASE(a, !=, b, size_t, "zu"); \
} while(0)


// ==============================================
// CHAR ASSERTIONS
// ==============================================

#define ASSERT_EQ_CHAR(a, b) \
do { \
    TYPE_CHECK(a, char); \
    TYPE_CHECK(b, char); \
    ASSERT_BASE(a, ==, b, char, "c"); \
} while(0)

#define ASSERT_NE_CHAR(a, b) \
do { \
    TYPE_CHECK(a, char); \
    TYPE_CHECK(b, char); \
    ASSERT_BASE(a, !=, b, char, "c"); \
} while(0)

#define ASSERT_EQ_UCHAR(a, b) \
do { \
    TYPE_CHECK(a, unsigned char); \
    TYPE_CHECK(b, unsigned char); \
    ASSERT_BASE(a, ==, b, unsigned char, "u"); \
} while(0)

#define ASSERT_NE_UCHAR(a, b) \
do { \
    TYPE_CHECK(a, unsigned char); \
    TYPE_CHECK(b, unsigned char); \
    ASSERT_BASE(a, !=, b, unsigned char, "u"); \
} while(0)


// ==============================================
// FLOAT ASSERTIONS (with epsilon)
// ==============================================

#define ASSERT_EQ_FLOAT(a, b, epsilon) \
do { \
    TYPE_CHECK(a, float); \
    TYPE_CHECK(b, float); \
    TYPE_CHECK(epsilon, float); \
    float const eval_a = (a); \
    float const eval_b = (b); \
    float const eval_eps = (epsilon); \
    if (!(fabsf(eval_a - eval_b) <= eval_eps)) { \
        fprintf(stderr, \
                "Assertion failed in %s on line %d: `%s == %s` " \
                "(%f == %f, diff: %f > %f)\n", \
                __FILE__, __LINE__, #a, #b, eval_a, eval_b, \
                fabsf(eval_a - eval_b), eval_eps); \
        abort(); \
    } \
} while(0)

#define ASSERT_NE_FLOAT(a, b, epsilon) \
do { \
    TYPE_CHECK(a, float); \
    TYPE_CHECK(b, float); \
    TYPE_CHECK(epsilon, float); \
    float const eval_a = (a); \
    float const eval_b = (b); \
    float const eval_eps = (epsilon); \
    if (!(fabsf(eval_a - eval_b) > eval_eps)) { \
        fprintf(stderr, \
                "Assertion failed in %s on line %d: `%s != %s` " \
                "(%f != %f, diff: %f <= %f)\n", \
                __FILE__, __LINE__, #a, #b, eval_a, eval_b, \
                fabsf(eval_a - eval_b), eval_eps); \
        abort(); \
    } \
} while(0)


// ==============================================
// DOUBLE ASSERTIONS (with epsilon)
// ==============================================

#define ASSERT_EQ_DOUBLE(a, b, epsilon) \
do { \
    TYPE_CHECK(a, double); \
    TYPE_CHECK(b, double); \
    TYPE_CHECK(epsilon, double); \
    double const eval_a = (a); \
    double const eval_b = (b); \
    double const eval_eps = (epsilon); \
    if (!(fabs(eval_a - eval_b) <= eval_eps)) { \
        fprintf(stderr, \
                "Assertion failed in %s on line %d: `%s == %s` " \
                "(%f == %f, diff: %f > %f)\n", \
                __FILE__, __LINE__, #a, #b, eval_a, eval_b, \
                fabs(eval_a - eval_b), eval_eps); \
        abort(); \
    } \
} while(0)

#define ASSERT_NE_DOUBLE(a, b, epsilon) \
do { \
    TYPE_CHECK(a, double); \
    TYPE_CHECK(b, double); \
    TYPE_CHECK(epsilon, double); \
    double const eval_a = (a); \
    double const eval_b = (b); \
    double const eval_eps = (epsilon); \
    if (!(fabs(eval_a - eval_b) > eval_eps)) { \
        fprintf(stderr, \
                "Assertion failed in %s on line %d: `%s != %s` " \
                "(%f != %f, diff: %f <= %f)\n", \
                __FILE__, __LINE__, #a, #b, eval_a, eval_b, \
                fabs(eval_a - eval_b), eval_eps); \
        abort(); \
    } \
} while(0)

#endif
