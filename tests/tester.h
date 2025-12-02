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

#ifndef TESTER_H
#define TESTER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>

// =============================================================================
// TEST RUNNER
// =============================================================================

enum TestStatus
{
    TEST_OK = 0,
    TEST_SKIP = 1
};

#define RETURN_OK()     \
    do                  \
    {                   \
        return TEST_OK; \
    } while (0)

#define RETURN_SKIP(explanation)              \
    do                                        \
    {                                         \
        fprintf(stderr, "%s\n", explanation); \
        fflush(stderr);                       \
        return TEST_SKIP;                     \
    } while (0)

#define RUN_TEST(test_func)                   \
    do                                        \
    {                                         \
        printf("Running %s... ", #test_func); \
        fflush(stdout);                       \
        int result = test_func();             \
        if (result == TEST_OK)                \
        {                                     \
            printf("PASSED\n");               \
        }                                     \
        else if (result == TEST_SKIP)         \
        {                                     \
            printf("SKIPPED\n");              \
        }                                     \
        else                                  \
        {                                     \
            printf("FAILED\n");               \
        }                                     \
    } while (0)

// =============================================================================
// BASE MACROS
// =============================================================================

#define ASSERT_BASE(a, operator, b, type, conv)                      \
    do                                                               \
    {                                                                \
        type const eval_a = (a);                                     \
        type const eval_b = (b);                                     \
        if (!(eval_a operator eval_b))                               \
        {                                                            \
            fprintf(stderr,                                          \
                    "Assertion failed in %s on line %d: `%s %s %s` " \
                    "(%" conv " %s %" conv ")\n",                    \
                    __FILE__, __LINE__, #a, #operator, #b,           \
                    eval_a, #operator, eval_b);                      \
            abort();                                                 \
        }                                                            \
    } while (0)

#define ASSERT_BASE_STR(expr, a, operator, b, type, conv)            \
    do                                                               \
    {                                                                \
        if (!(expr))                                                 \
        {                                                            \
            fprintf(stderr,                                          \
                    "Assertion failed in %s on line %d: `%s %s %s` " \
                    "(%" conv " %s %" conv ")\n",                    \
                    __FILE__, __LINE__, #a, #operator, #b,           \
                    (type)a, #operator,(type) b);                    \
            abort();                                                 \
        }                                                            \
    } while (0)

// =============================================================================
// BOOLEAN ASSERTIONS
// =============================================================================

#define ASSERT_TRUE(a) ASSERT_BASE(a, ==, true, bool, "d")
#define ASSERT_FALSE(a) ASSERT_BASE(a, ==, false, bool, "d")

// =============================================================================
// NULL ASSERTIONS
// =============================================================================

#define ASSERT_NULL(a) ASSERT_BASE(a, ==, NULL, const void *, "p")
#define ASSERT_NOT_NULL(a) ASSERT_BASE(a, !=, NULL, const void *, "p")

// =============================================================================
// INTEGER ASSERTIONS
// =============================================================================

#define ASSERT_EQ(a, b) ASSERT_BASE(a, ==, b, int64_t, PRId64)
#define ASSERT_NE(a, b) ASSERT_BASE(a, !=, b, int64_t, PRId64)
#define ASSERT_GT(a, b) ASSERT_BASE(a, >, b, int64_t, PRId64)
#define ASSERT_LE(a, b) ASSERT_BASE(a, <=, b, int64_t, PRId64)

// =============================================================================
// STRING ASSERTIONS
// =============================================================================

#define ASSERT_EQ_STR(a, b) \
    ASSERT_BASE_STR(strcmp(a, b) == 0, a, ==, b, char *, "s")

#endif
