/* Runtime loading without a vendor runtime. SPDX-License-Identifier: MIT */
#include "runtime_loader.h"
#include "uscale_test.h"

static void check_symbol(uscale_runtime *runtime)
{
    void *symbol = uscale_runtime_symbol(runtime, "uscale_runtime_test_value");
    int (*value)(void) = NULL;
    /* POSIX and Windows support calling a loaded function address. Avoid a
     * function/object pointer cast so the test also builds with -Wpedantic. */
    _Static_assert(sizeof(value) == sizeof(symbol), "symbol pointer size");
    CHECK(symbol != NULL);
    if (symbol != NULL) {
        memcpy(&value, &symbol, sizeof(value));
        CHECK_INT(value(), 42);
    }
}

int main(int argc, char **argv)
{
    const char *missing[] = {"uscale_test_missing_runtime_17_a", "uscale_test_missing_runtime_17_b"};
    const char *invalid[] = {NULL};
    const char *empty[] = {""};
    uscale_runtime *runtime = NULL;
    uscale_runtime *second = NULL;
    CHECK_INT(argc, 2);
    if (argc == 2) {
        const char *first[] = {argv[1], missing[0]};
        const char *fallback[] = {missing[0], argv[1]};
        CHECK_INT(uscale_runtime_open(first, 2, &runtime), USCALE_OK);
        CHECK(runtime != NULL);
        check_symbol(runtime);
        CHECK(uscale_runtime_symbol(runtime, "uscale_test_missing_symbol_17") == NULL);
        CHECK(uscale_runtime_symbol(runtime, NULL) == NULL);
        CHECK(uscale_runtime_symbol(runtime, "") == NULL);
        CHECK_INT(uscale_runtime_open(fallback, 2, &second), USCALE_OK);
        CHECK(second != NULL);
        uscale_runtime_close(runtime);
        check_symbol(second);
        /* A failed open clears the output without disturbing another handle. */
        runtime = second;
        CHECK_INT(uscale_runtime_open(missing, 2, &runtime), USCALE_ERROR_RUNTIME_MISSING);
        CHECK(runtime == NULL);
        check_symbol(second);
        uscale_runtime_close(second);
    }
    CHECK_INT(uscale_runtime_open(missing, 2, &runtime), USCALE_ERROR_RUNTIME_MISSING);
    CHECK(runtime == NULL);
    CHECK_INT(uscale_runtime_open(NULL, 1, &runtime), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(uscale_runtime_open(missing, 0, &runtime), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(uscale_runtime_open(invalid, 1, &runtime), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(uscale_runtime_open(empty, 1, &runtime), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(uscale_runtime_open(missing, 2, NULL), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK(runtime == NULL);
    CHECK(uscale_runtime_symbol(NULL, "uscale_runtime_test_value") == NULL);
    uscale_runtime_close(NULL);
    USCALE_TEST_END;
}
