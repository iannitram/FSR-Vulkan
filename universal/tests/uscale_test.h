/* Minimal test harness for uscale's C tests: no framework, one main per file.
 * A test file calls the CHECK macros, then ends main with USCALE_TEST_END.
 * SPDX-License-Identifier: MIT */
#ifndef USCALE_TEST_H
#define USCALE_TEST_H

#include <stdio.h>
#include <string.h>

static int uscale_test_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__,   \
                    #cond);                                                    \
            uscale_test_failures++;                                            \
        }                                                                      \
    } while (0)

#define CHECK_INT(got, want)                                                   \
    do {                                                                       \
        long long g_ = (long long)(got);                                       \
        long long w_ = (long long)(want);                                      \
        if (g_ != w_) {                                                        \
            fprintf(stderr, "%s:%d: %s: expected %lld, got %lld\n", __FILE__,  \
                    __LINE__, #got, w_, g_);                                   \
            uscale_test_failures++;                                            \
        }                                                                      \
    } while (0)

#define CHECK_STR(got, want)                                                   \
    do {                                                                       \
        const char *g_ = (got);                                                \
        const char *w_ = (want);                                               \
        if (g_ == NULL || strcmp(g_, w_) != 0) {                               \
            fprintf(stderr, "%s:%d: %s: expected \"%s\", got \"%s\"\n",        \
                    __FILE__, __LINE__, #got, w_, g_ ? g_ : "(null)");         \
            uscale_test_failures++;                                            \
        }                                                                      \
    } while (0)

#define USCALE_TEST_END                                                        \
    do {                                                                       \
        if (uscale_test_failures != 0) {                                       \
            fprintf(stderr, "%d check(s) failed\n", uscale_test_failures);     \
            return 1;                                                          \
        }                                                                      \
        return 0;                                                              \
    } while (0)

#endif
