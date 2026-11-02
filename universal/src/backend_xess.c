/* Backend XESS: Intel's XeSS runtime, loaded at run time.
 * Stub (code map: universal/src, planned). SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "backend.h"

static uscale_result xess_create(const uscale_context_create_info *info, void **out_state)
{
    (void)info;
    if (out_state != NULL) {
        *out_state = NULL;
    }
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

static uscale_result xess_record_upscale(void *state, const uscale_upscale_info *info)
{
    (void)state;
    (void)info;
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

static void xess_destroy(void *state)
{
    (void)state;
}

static const uscale_backend_ops xess_ops = {
    USCALE_BACKEND_XESS, xess_create, xess_record_upscale, xess_destroy
};

const uscale_backend_ops *uscale_backend_xess_ops(void)
{
    return &xess_ops;
}
