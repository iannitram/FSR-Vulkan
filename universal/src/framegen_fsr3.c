/* Frame generation: upstream FSR 3.1.6 (ffx-vulkan::fsr3-vk-framegeneration-3.1.6)
 * plus its presenter policy. Stub (code map: universal/src, planned). SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "backend.h"

uscale_result uscale_framegen_create(const uscale_context_create_info *info, void **out_state)
{
    (void)info;
    if (out_state != NULL) {
        *out_state = NULL;
    }
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

uscale_result uscale_framegen_record(void *state, const uscale_framegen_info *info)
{
    (void)state;
    (void)info;
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

uscale_result uscale_framegen_retire(void *state, uint64_t frame_id)
{
    (void)state;
    (void)frame_id;
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

void uscale_framegen_destroy(void *state)
{
    (void)state;
}
