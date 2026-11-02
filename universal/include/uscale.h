/*
 * uscale.h - the stable C API of our shared upscaler and frame-generation
 * library (SPEC.md M1-M3, M9). This header has no Vulkan types, so it builds
 * on every platform without the Vulkan SDK. The Vulkan half is uscale_vulkan.h.
 *
 * ABI rules: enums only grow at the end; structs start with struct_size and
 * only grow at the end; a removed or changed call bumps USCALE_VERSION_MAJOR.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef USCALE_H
#define USCALE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USCALE_VERSION_MAJOR 0u
#define USCALE_VERSION_MINOR 1u
#define USCALE_VERSION_PATCH 0u

/* Packed like Vulkan: major << 22 | minor << 12 | patch. */
#define USCALE_MAKE_VERSION(major, minor, patch) \
    ((((uint32_t)(major)) << 22u) | (((uint32_t)(minor)) << 12u) | ((uint32_t)(patch)))
#define USCALE_VERSION \
    USCALE_MAKE_VERSION(USCALE_VERSION_MAJOR, USCALE_VERSION_MINOR, USCALE_VERSION_PATCH)

typedef enum uscale_result {
    USCALE_OK = 0,
    USCALE_ERROR_INVALID_ARGUMENT = -1,
    USCALE_ERROR_UNSUPPORTED = -2,
    USCALE_ERROR_NOT_IMPLEMENTED = -3,
    USCALE_ERROR_OUT_OF_MEMORY = -4,
    USCALE_ERROR_VULKAN = -5,
    USCALE_ERROR_VERSION_MISMATCH = -6,
    USCALE_ERROR_RUNTIME_MISSING = -7
} uscale_result;

typedef enum uscale_backend {
    USCALE_BACKEND_AUTO = 0,
    USCALE_BACKEND_FSR3 = 1,
    USCALE_BACKEND_FSR4 = 2,
    USCALE_BACKEND_XESS = 3,
    USCALE_BACKEND_DLSS = 4,
    USCALE_BACKEND_COUNT = 5
} uscale_backend;

typedef enum uscale_quality {
    USCALE_QUALITY_NATIVE_AA = 0,
    USCALE_QUALITY_QUALITY = 1,
    USCALE_QUALITY_BALANCED = 2,
    USCALE_QUALITY_PERFORMANCE = 3,
    USCALE_QUALITY_ULTRA_PERFORMANCE = 4,
    USCALE_QUALITY_COUNT = 5
} uscale_quality;

typedef struct uscale_extent {
    uint32_t width;
    uint32_t height;
} uscale_extent;

/* PCI vendor ids used by backend choice. */
#define USCALE_VENDOR_AMD 0x1002u
#define USCALE_VENDOR_NVIDIA 0x10DEu
#define USCALE_VENDOR_INTEL 0x8086u

/* What the device and the player's machine offer. Filled by
 * uscale_query_caps() (uscale_vulkan.h) or by hand in tests. */
typedef struct uscale_device_caps {
    uint32_t struct_size;            /* sizeof(uscale_device_caps) */
    uint32_t vendor_id;              /* PCI vendor id of the GPU */
    uint32_t has_int_dot_product;    /* VK_KHR_shader_integer_dot_product usable: 1 or 0 */
    uint32_t has_fp16;               /* shaderFloat16 usable: 1 or 0 */
    uint32_t is_moltenvk;            /* running on MoltenVK (Mac): 1 or 0 */
    uint32_t fsr4_assets_available;  /* the game supplied a valid FSR 4 asset folder: 1 or 0 */
    uint32_t xess_runtime_available; /* Intel's XeSS runtime was found and loaded: 1 or 0 */
    uint32_t dlss_runtime_available; /* NVIDIA's DLSS runtime was found and loaded: 1 or 0 */
} uscale_device_caps;

/* Profiling switches (SPEC.md M9), from BB_FSR4_PROFILE and BB_FSR4_STATS. */
#define USCALE_PROFILE_GPU_TIMES 0x1u  /* BB_FSR4_PROFILE */
#define USCALE_PROFILE_PIPELINE_STATS 0x2u /* BB_FSR4_STATS */

/* The packed version of the library actually linked (compare with USCALE_VERSION). */
uint32_t uscale_version(void);

/* Constant English name of a result, e.g. "USCALE_OK"; never NULL. */
const char *uscale_result_name(uscale_result result);

/* Constant short name of a backend, e.g. "FSR 3"; never NULL. */
const char *uscale_backend_name(uscale_backend backend);

/* Render size for a quality preset and display size. */
uscale_result uscale_render_extent(uscale_quality quality, uscale_extent display,
                                   uscale_extent *out_render);

/* Picks the backend to run. preferred = USCALE_BACKEND_AUTO picks the best
 * available; a named backend that cannot run falls back to FSR 3. *out_reason
 * gets a constant English sentence saying why (may be NULL to skip). */
uscale_result uscale_select_backend(const uscale_device_caps *caps, uscale_backend preferred,
                                    uscale_backend *out_backend, const char **out_reason);

/* Profiling flags from the two environment values (pass getenv() results;
 * NULL means unset). */
uint32_t uscale_profile_flags_from_env(const char *bb_fsr4_profile, const char *bb_fsr4_stats);

#ifdef __cplusplus
}
#endif

#endif /* USCALE_H */
