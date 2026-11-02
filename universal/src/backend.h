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

/* Frame generation (upstream FSR 3.1.6 + presenter policy). */
uscale_result uscale_framegen_create(const uscale_context_create_info *info, void **out_state);
uscale_result uscale_framegen_record(void *state, const uscale_framegen_info *info);
uscale_result uscale_framegen_retire(void *state, uint64_t frame_id);
void uscale_framegen_destroy(void *state);

#endif
