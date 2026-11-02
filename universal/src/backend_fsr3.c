/* Backend FSR3: upstream FSR 3.1.x upscaler (ffx-vulkan::fsr3-vk-backend-1.1.4).
 * Stub (code map: universal/src, planned). SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "backend.h"

static uscale_result fsr3_create(const uscale_context_create_info *info, void **out_state)
{
    (void)info;
    if (out_state != NULL) {
        *out_state = NULL;
    }
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

static uscale_result fsr3_record_upscale(void *state, const uscale_upscale_info *info)
{
    (void)state;
    (void)info;
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

static void fsr3_destroy(void *state)
{
    (void)state;
}

static const uscale_backend_ops fsr3_ops = {
    USCALE_BACKEND_FSR3, fsr3_create, fsr3_record_upscale, fsr3_destroy
};

const uscale_backend_ops *uscale_backend_fsr3_ops(void)
{
    return &fsr3_ops;
}
