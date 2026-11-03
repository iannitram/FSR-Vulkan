/* uscale_select_backend: present (code map: universal/src, roadmap 2.03). SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "uscale.h"

/* Each block_* returns the first failing check's reason, or NULL when the backend is available. */

static const char *block_fsr4(const uscale_device_caps *caps)
{
    if (!caps->fsr4_assets_available)
        return "FSR 4 unavailable: no FSR 4 assets";
    if (!caps->has_int_dot_product)
        return "FSR 4 unavailable: no integer dot product";
    if (!caps->has_fp16)
        return "FSR 4 unavailable: no FP16";
    return NULL;
}

static const char *block_xess(const uscale_device_caps *caps)
{
    if (caps->is_moltenvk)
        return "XeSS unavailable: no XeSS runtime on MoltenVK";
    if (!caps->xess_runtime_available)
        return "XeSS unavailable: XeSS runtime not found";
    if (!caps->has_int_dot_product)
        return "XeSS unavailable: no integer dot product";
    return NULL;
}

static const char *block_dlss(const uscale_device_caps *caps)
{
    if (caps->is_moltenvk)
        return "DLSS unavailable: no DLSS runtime on MoltenVK";
    if (caps->vendor_id != USCALE_VENDOR_NVIDIA)
        return "DLSS unavailable: not an NVIDIA GPU";
    if (!caps->dlss_runtime_available)
        return "DLSS unavailable: DLSS runtime not found";
    return NULL;
}

static const char *block_reason(const uscale_device_caps *caps, uscale_backend backend)
{
    switch (backend) {
    case USCALE_BACKEND_FSR4:
        return block_fsr4(caps);
    case USCALE_BACKEND_XESS:
        return block_xess(caps);
    case USCALE_BACKEND_DLSS:
        return block_dlss(caps);
    default:
        return NULL; /* FSR 3 is always available. */
    }
}

/* AUTO order: DLSS, FSR 4, XeSS, FSR 3. Every entry is a string literal. */
static const struct {
    uscale_backend backend;
    const char *auto_reason;
    const char *requested_reason;
} k_entries[] = {
    {USCALE_BACKEND_DLSS, "auto: DLSS is the best available", "requested: DLSS"},
    {USCALE_BACKEND_FSR4, "auto: FSR 4 is the best available", "requested: FSR 4"},
    {USCALE_BACKEND_XESS, "auto: XeSS is the best available", "requested: XeSS"},
    {USCALE_BACKEND_FSR3, "auto: FSR 3 is the best available", "requested: FSR 3"},
};

uscale_result uscale_select_backend(const uscale_device_caps *caps, uscale_backend preferred,
                                    uscale_backend *out_backend, const char **out_reason)
{
    const size_t n = sizeof k_entries / sizeof k_entries[0];
    size_t i;
    uscale_backend chosen;
    const char *reason;

    if (caps == NULL || out_backend == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    if (caps->struct_size < sizeof(uscale_device_caps))
        return USCALE_ERROR_VERSION_MISMATCH;
    if ((int)preferred < 0 || (int)preferred >= (int)USCALE_BACKEND_COUNT)
        return USCALE_ERROR_INVALID_ARGUMENT;

    if (preferred == USCALE_BACKEND_AUTO) {
        /* The last entry is FSR 3, which is always available, so the scan stops in range. */
        for (i = 0; i + 1 < n && block_reason(caps, k_entries[i].backend) != NULL; i++)
            ;
        chosen = k_entries[i].backend;
        reason = k_entries[i].auto_reason;
    } else {
        const char *block = block_reason(caps, preferred);

        if (block == NULL) {
            /* Named backend is available: use it. preferred is 1..4 here, so an entry exists. */
            for (i = 0; i < n && k_entries[i].backend != preferred; i++)
                ;
            chosen = preferred;
            reason = k_entries[i].requested_reason;
        } else {
            chosen = USCALE_BACKEND_FSR3;
            reason = block;
        }
    }

    *out_backend = chosen;
    if (out_reason != NULL)
        *out_reason = reason;
    return USCALE_OK;
}
