/* Backend DLSS: NVIDIA's DLSS runtime, loaded at run time.
 * Stub (code map: universal/src, planned). SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "backend.h"

static uscale_result dlss_create(const uscale_context_create_info *info, void **out_state)
{
    (void)info;
    if (out_state != NULL) {
        *out_state = NULL;
    }
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

static uscale_result dlss_record_upscale(void *state, const uscale_upscale_info *info)
{
    (void)state;
    (void)info;
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

static void dlss_destroy(void *state)
{
    (void)state;
}

static const uscale_backend_ops dlss_ops = {
    USCALE_BACKEND_DLSS, dlss_create, dlss_record_upscale, dlss_destroy
};

const uscale_backend_ops *uscale_backend_dlss_ops(void)
{
    return &dlss_ops;
}
