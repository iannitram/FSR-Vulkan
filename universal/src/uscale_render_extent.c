/* uscale_render_extent: present (code map: universal/src, planned). SPDX-License-Identifier: MIT */
#include <stddef.h>
#include <stdint.h>

#include "uscale.h"

/* Render size as num/den of the display size, per quality preset (indexed by uscale_quality). */
static const struct {
    uint32_t num;
    uint32_t den;
} k_scale[USCALE_QUALITY_COUNT] = {
    {1u, 1u},   /* NATIVE_AA */
    {2u, 3u},   /* QUALITY */
    {10u, 17u}, /* BALANCED */
    {1u, 2u},   /* PERFORMANCE */
    {1u, 3u},   /* ULTRA_PERFORMANCE */
};

static uint32_t scale_axis(uint32_t size, uint32_t num, uint32_t den)
{
    /* 64-bit product so a 32-bit size cannot overflow; integer division floors. */
    uint64_t v = (uint64_t)size * num / den;
    if (v == 0) {
        v = 1; /* minimum of 1 on each side */
    }
    return (uint32_t)v;
}

uscale_result uscale_render_extent(uscale_quality quality, uscale_extent display,
                                   uscale_extent *out_render)
{
    uscale_extent render;

    if (out_render == NULL) {
        return USCALE_ERROR_INVALID_ARGUMENT;
    }
    if (display.width == 0 || display.height == 0) {
        return USCALE_ERROR_INVALID_ARGUMENT;
    }
    if ((int)quality < 0 || (int)quality >= (int)USCALE_QUALITY_COUNT) {
        return USCALE_ERROR_INVALID_ARGUMENT;
    }

    render.width = scale_axis(display.width, k_scale[quality].num, k_scale[quality].den);
    render.height = scale_axis(display.height, k_scale[quality].num, k_scale[quality].den);
    *out_render = render;
    return USCALE_OK;
}
