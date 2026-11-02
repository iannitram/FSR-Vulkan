/* Backend FSR4: upstream FSR 4 v07 provider (ffx-vulkan::fsr4-v07-vulkan) with game-supplied assets.
 * Stub (code map: universal/src, planned). SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "backend.h"

static uscale_result fsr4_create(const uscale_context_create_info *info, void **out_state)
{
    (void)info;
    if (out_state != NULL) {
        *out_state = NULL;
    }
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

static uscale_result fsr4_record_upscale(void *state, const uscale_upscale_info *info)
{
    (void)state;
    (void)info;
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

static void fsr4_destroy(void *state)
{
    (void)state;
}

static const uscale_backend_ops fsr4_ops = {
    USCALE_BACKEND_FSR4, fsr4_create, fsr4_record_upscale, fsr4_destroy
};

const uscale_backend_ops *uscale_backend_fsr4_ops(void)
{
    return &fsr4_ops;
}
