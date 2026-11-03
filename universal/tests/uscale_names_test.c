/* Locked test for roadmap 2.01 (result and backend names). Do not edit.
 * SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "uscale.h"
#include "uscale_test.h"

static const struct { int value; const char *name; } k_results[] = {
    {USCALE_OK, "USCALE_OK"},
    {USCALE_ERROR_INVALID_ARGUMENT, "USCALE_ERROR_INVALID_ARGUMENT"},
    {USCALE_ERROR_UNSUPPORTED, "USCALE_ERROR_UNSUPPORTED"},
    {USCALE_ERROR_NOT_IMPLEMENTED, "USCALE_ERROR_NOT_IMPLEMENTED"},
    {USCALE_ERROR_OUT_OF_MEMORY, "USCALE_ERROR_OUT_OF_MEMORY"},
    {USCALE_ERROR_VULKAN, "USCALE_ERROR_VULKAN"},
    {USCALE_ERROR_VERSION_MISMATCH, "USCALE_ERROR_VERSION_MISMATCH"},
    {USCALE_ERROR_RUNTIME_MISSING, "USCALE_ERROR_RUNTIME_MISSING"},
};

static const struct { int value; const char *name; } k_backends[] = {
    {USCALE_BACKEND_AUTO, "Auto"},
    {USCALE_BACKEND_FSR3, "FSR 3"},
    {USCALE_BACKEND_FSR4, "FSR 4"},
    {USCALE_BACKEND_XESS, "XeSS"},
    {USCALE_BACKEND_DLSS, "DLSS"},
};

int main(void)
{
    size_t i;

    /* Expectation 1: each result value gives its exact enum identifier. */
    for (i = 0; i < sizeof(k_results) / sizeof(k_results[0]); ++i) {
        CHECK_STR(uscale_result_name((uscale_result)k_results[i].value), k_results[i].name);
    }

    /* Expectation 2: any other result value gives "USCALE_ERROR_UNKNOWN". */
    CHECK_STR(uscale_result_name((uscale_result)1), "USCALE_ERROR_UNKNOWN");
    CHECK_STR(uscale_result_name((uscale_result)-8), "USCALE_ERROR_UNKNOWN");
    CHECK_STR(uscale_result_name((uscale_result)12345), "USCALE_ERROR_UNKNOWN");

    /* Expectation 3: each backend gives its display name. */
    for (i = 0; i < sizeof(k_backends) / sizeof(k_backends[0]); ++i) {
        CHECK_STR(uscale_backend_name((uscale_backend)k_backends[i].value), k_backends[i].name);
    }

    /* Expectation 4: USCALE_BACKEND_COUNT and any other value give "Unknown". */
    CHECK_STR(uscale_backend_name(USCALE_BACKEND_COUNT), "Unknown");
    CHECK_STR(uscale_backend_name((uscale_backend)-1), "Unknown");
    CHECK_STR(uscale_backend_name((uscale_backend)99), "Unknown");

    /* Expectation 5: never NULL, and the same constant string on every call. */
    CHECK(uscale_result_name((uscale_result)777) != NULL);
    CHECK(uscale_backend_name((uscale_backend)777) != NULL);
    CHECK(uscale_result_name(USCALE_OK) == uscale_result_name(USCALE_OK));
    CHECK(uscale_backend_name(USCALE_BACKEND_DLSS) == uscale_backend_name(USCALE_BACKEND_DLSS));

    USCALE_TEST_END;
}
