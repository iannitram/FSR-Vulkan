/* Test-only shared library; no vendor runtime. SPDX-License-Identifier: MIT */
#if defined(_WIN32)
#define USCALE_TEST_EXPORT __declspec(dllexport)
#else
#define USCALE_TEST_EXPORT __attribute__((visibility("default")))
#endif

USCALE_TEST_EXPORT int uscale_runtime_test_value(void)
{
    return 42;
}
