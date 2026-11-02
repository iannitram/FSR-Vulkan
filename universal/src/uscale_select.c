/* uscale_select_backend: stub (code map: universal/src, planned). SPDX-License-Identifier: MIT */
#include "uscale.h"

uscale_result uscale_select_backend(const uscale_device_caps *caps, uscale_backend preferred,
                                    uscale_backend *out_backend, const char **out_reason)
{
    (void)caps;
    (void)preferred;
    (void)out_backend;
    (void)out_reason;
    return USCALE_ERROR_NOT_IMPLEMENTED;
}
