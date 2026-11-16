/* Internal backend table: one entry per backend module. Not part of the API.
 * SPDX-License-Identifier: MIT */
#ifndef USCALE_BACKEND_H
#define USCALE_BACKEND_H

#include "uscale_vulkan.h"

typedef struct uscale_backend_ops {
    uscale_backend id;
    /* Creates the backend's state for a context; *out_state is owned by the backend. */
    uscale_result (*create)(const uscale_context_create_info *info, void **out_state);
    uscale_result (*record_upscale)(void *state, const uscale_upscale_info *info);
    void (*destroy)(void *state);
} uscale_backend_ops;

const uscale_backend_ops *uscale_backend_fsr3_ops(void);
const uscale_backend_ops *uscale_backend_fsr4_ops(void);
const uscale_backend_ops *uscale_backend_xess_ops(void);
const uscale_backend_ops *uscale_backend_dlss_ops(void);

/* Frame generation as a table, so a test can swap it like a backend. */
typedef struct uscale_framegen_ops {
    uscale_result (*create)(const uscale_context_create_info *info, void **out_state);
    uscale_result (*prepare)(void *state, const uscale_framegen_prepare_info *info);
    uscale_result (*dispatch)(void *state, const uscale_framegen_dispatch_info *info,
                              uscale_present_plan *out_plan);
    uscale_result (*retire)(void *state, uint64_t frame_id);
    void (*destroy)(void *state);
} uscale_framegen_ops;

/* Frame generation (upstream FSR 3.1.6 + presenter policy). create is called
 * only with a supported framegen_color_format; without USCALE_WITH_FFX it
 * succeeds with no state and prepare/dispatch return USCALE_ERROR_UNSUPPORTED. */
uscale_result uscale_framegen_create(const uscale_context_create_info *info, void **out_state);
uscale_result uscale_framegen_prepare(void *state, const uscale_framegen_prepare_info *info);
uscale_result uscale_framegen_dispatch(void *state, const uscale_framegen_dispatch_info *info,
                                       uscale_present_plan *out_plan);
uscale_result uscale_framegen_retire(void *state, uint64_t frame_id);
void uscale_framegen_destroy(void *state);

#ifdef USCALE_HAVE_FFX
#include "ffx_vk_fsr3_3_1_5_bridge.h"

/* Test seam for framegen_fsr3.c: upstream's FSR 3.1.6 frame-generation calls,
 * so validation and plan tests run without a GPU. NULL restores upstream's.
 * Read at uscale_framegen_create; not thread-safe. */
typedef struct uscale_fsr3fg_upstream {
    FfxVkFsr3_3_1_6FrameGenerationResult (*create)(
        const FfxVkFsr3_3_1_6FrameGenerationCreateInfo *info,
        FfxVkFsr3_3_1_6FrameGenerationContext **out_context);
    FfxVkFsr3_3_1_6FrameGenerationResult (*prepare)(
        FfxVkFsr3_3_1_6FrameGenerationContext *context,
        const FfxVkFsr3_3_1_6FrameGenerationPrepareInfo *info);
    FfxVkFsr3_3_1_6FrameGenerationResult (*dispatch)(
        FfxVkFsr3_3_1_6FrameGenerationContext *context,
        const FfxVkFsr3_3_1_6FrameGenerationDispatchInfo *info);
    FfxVkFsr3_3_1_6FrameGenerationResult (*retire)(
        FfxVkFsr3_3_1_6FrameGenerationContext *context, uint64_t completed_frame_id);
    void (*destroy)(FfxVkFsr3_3_1_6FrameGenerationContext *context);
} uscale_fsr3fg_upstream;

void uscale_framegen_fsr3_set_upstream(const uscale_fsr3fg_upstream *upstream);
#endif

/* Test seam, not part of the API and not exported by any public header.
 * A NULL member keeps the real behaviour. Set before uscale_context_create
 * and reset with NULL; not thread-safe. */
typedef struct uscale_context_hooks {
    /* Replaces the backend table lookup (id is FSR3, FSR4, XESS or DLSS). */
    const uscale_backend_ops *(*backend_ops)(uscale_backend id);
    /* Replaces uscale_query_caps, which needs a real device. */
    uscale_result (*query_caps)(const uscale_context_create_info *info,
                                uscale_device_caps *out_caps);
    /* Replaces the frame generation functions above. */
    const uscale_framegen_ops *framegen;
} uscale_context_hooks;

void uscale_context_set_hooks(const uscale_context_hooks *hooks);

#endif
