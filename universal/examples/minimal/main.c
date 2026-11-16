/* A tiny consumer of the stable C API. SPDX-License-Identifier: MIT */
#include <stdio.h>

#include "uscale.h"

int main(void)
{
    const uscale_extent display = { 1920u, 1080u };
    uscale_extent render = { 0u, 0u };
    const uscale_result result = uscale_render_extent(
        USCALE_QUALITY_QUALITY, display, &render);

    if (result != USCALE_OK) {
        fprintf(stderr, "uscale_render_extent: %s\n", uscale_result_name(result));
        return 1;
    }

    printf("uscale %u.%u.%u: render %ux%u for %ux%u display\n",
           USCALE_VERSION_MAJOR, USCALE_VERSION_MINOR, USCALE_VERSION_PATCH,
           render.width, render.height, display.width, display.height);
    return 0;
}
