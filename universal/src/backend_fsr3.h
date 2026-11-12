/* Private FSR 3 dispatch mapping; no FFX runtime dependency.
 * SPDX-License-Identifier: MIT */
#ifndef USCALE_BACKEND_FSR3_H
#define USCALE_BACKEND_FSR3_H

#include "uscale_vulkan.h"
#include "ffx_vk_portable.h"

/* Pure mapping, no allocation or Vulkan calls. The caller supplies its frame
 * counter and validates image capabilities/numeric ranges before dispatch.
 * Images describe single-mip, single-layer 2D resources with sampled inputs
 * and a storage output (the usages required by uscale). Unsupported layouts
 * return USCALE_ERROR_UNSUPPORTED; on any error, *out_dispatch is unchanged. */
uscale_result uscale_fsr3_map_dispatch(const uscale_upscale_info *info,
                                      uint64_t frame_id,
                                      FfxVkPortableUpscaleDispatchInfo *out_dispatch);

#endif
