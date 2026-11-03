/* Locked test for roadmap 2.02 (render size per quality preset). Do not edit.
 * SPDX-License-Identifier: MIT */
#include "uscale.h"
#include "uscale_test.h"

static const struct {
    int quality;
    uint32_t dw, dh, rw, rh;
} k_cases[] = {
    /* 2560x1440 through every preset */
    {USCALE_QUALITY_NATIVE_AA, 2560, 1440, 2560, 1440},
    {USCALE_QUALITY_QUALITY, 2560, 1440, 1706, 960},
    {USCALE_QUALITY_BALANCED, 2560, 1440, 1505, 847},
    {USCALE_QUALITY_PERFORMANCE, 2560, 1440, 1280, 720},
    {USCALE_QUALITY_ULTRA_PERFORMANCE, 2560, 1440, 853, 480},
    /* floor, never round */
    {USCALE_QUALITY_QUALITY, 1280, 800, 853, 533},
    {USCALE_QUALITY_BALANCED, 3840, 2160, 2258, 1270},
    /* a result of 0 becomes 1 */
    {USCALE_QUALITY_ULTRA_PERFORMANCE, 1, 1, 1, 1},
    {USCALE_QUALITY_PERFORMANCE, 1, 5, 1, 2},
    /* 64-bit math: no overflow at the largest width */
    {USCALE_QUALITY_PERFORMANCE, 4294967295u, 2, 2147483647u, 1},
    {USCALE_QUALITY_QUALITY, 4294967295u, 3, 2863311530u, 2},
};

int main(void)
{
    unsigned i;
    uscale_extent out;
    const uscale_extent good = {1920, 1080};
    const uscale_extent zero_w = {0, 1080};
    const uscale_extent zero_h = {1920, 0};

    /* Expectations 1-3: ratio per preset, floor with 64-bit math, minimum 1. */
    for (i = 0; i < sizeof(k_cases) / sizeof(k_cases[0]); ++i) {
        const uscale_extent display = {k_cases[i].dw, k_cases[i].dh};
        out.width = 7u;
        out.height = 7u;
        CHECK_INT(uscale_render_extent((uscale_quality)k_cases[i].quality, display, &out), USCALE_OK);
        CHECK_INT(out.width, k_cases[i].rw);
        CHECK_INT(out.height, k_cases[i].rh);
    }

    /* Expectation 4: NULL output is USCALE_ERROR_INVALID_ARGUMENT. */
    CHECK_INT(uscale_render_extent(USCALE_QUALITY_QUALITY, good, NULL), USCALE_ERROR_INVALID_ARGUMENT);

    /* Expectation 5: a zero display width or height is invalid, output untouched. */
    out.width = 11u;
    out.height = 22u;
    CHECK_INT(uscale_render_extent(USCALE_QUALITY_QUALITY, zero_w, &out), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(uscale_render_extent(USCALE_QUALITY_QUALITY, zero_h, &out), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(out.width, 11u);
    CHECK_INT(out.height, 22u);

    /* Expectation 6: a quality outside 0..4 is invalid, output untouched. */
    CHECK_INT(uscale_render_extent(USCALE_QUALITY_COUNT, good, &out), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(uscale_render_extent((uscale_quality)-1, good, &out), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(out.width, 11u);
    CHECK_INT(out.height, 22u);

    USCALE_TEST_END;
}
