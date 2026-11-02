/* Context lifecycle and dispatch through the backend table: stub
 * (code map: universal/src, planned). SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "backend.h"

uscale_result uscale_context_create(const uscale_context_create_info *info,
                                    uscale_context **out_context)
{
    (void)info;
    if (out_context != NULL) {
        *out_context = NULL;
    }
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

uscale_backend uscale_context_backend(const uscale_context *context, const char **out_reason)
{
    (void)context;
    if (out_reason != NULL) {
        *out_reason = "not implemented";
    }
    return USCALE_BACKEND_AUTO;
}

uscale_result uscale_record_upscale(uscale_context *context, const uscale_upscale_info *info)
{
    (void)context;
    (void)info;
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

uscale_result uscale_record_framegen(uscale_context *context, const uscale_framegen_info *info)
{
    (void)context;
    (void)info;
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

uscale_result uscale_retire_frame(uscale_context *context, uint64_t frame_id)
{
    (void)context;
    (void)frame_id;
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

void uscale_context_destroy(uscale_context *context)
{
    (void)context;
}
