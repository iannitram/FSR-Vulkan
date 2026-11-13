/*
 * uscale_vulkan.h - the Vulkan half of the stable C API (SPEC.md M1, M4-M8).
 * The game owns the instance, device, queues, command buffers, swapchain and
 * images; the library records work into the game's command buffer and never
 * submits or presents.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef USCALE_VULKAN_H
#define USCALE_VULKAN_H

#include <vulkan/vulkan.h>

#include "uscale.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uscale_context uscale_context;

typedef struct uscale_image {
    VkImage image;
    VkImageView view;   /* may be VK_NULL_HANDLE: the library makes its own */
    VkFormat format;
    uscale_extent extent;
    VkImageLayout layout; /* layout the image is in when the work is recorded */
} uscale_image;

/* Device requirements for the FSR 3 backend: the instance and device must use
 * Vulkan 1.2 or later (the upscale shaders are SPIR-V 1.5), and the game must
 * create `device` with the core feature shaderStorageImageWriteWithoutFormat
 * enabled (the upscale shaders write the output image as a storage image
 * without a format). The library does not check this; without it the
 * behaviour is undefined. */
typedef struct uscale_context_create_info {
    uint32_t struct_size;          /* sizeof(uscale_context_create_info) */
    uint32_t api_version;          /* USCALE_VERSION the game was built against */
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    PFN_vkGetInstanceProcAddr get_instance_proc_addr;
    PFN_vkGetDeviceProcAddr get_device_proc_addr;
    uscale_backend preferred_backend; /* USCALE_BACKEND_AUTO for the best available */
    uscale_quality quality;
    uscale_extent max_render_extent;
    uscale_extent display_extent;
    uint32_t enable_frame_generation; /* 1 to create frame generation too */
    const char *fsr4_asset_dir;        /* folder with the game's FSR 4 assets, or NULL */
    uint32_t hdr;                      /* colour input is HDR linear: 1 or 0 */
} uscale_context_create_info;

typedef struct uscale_upscale_info {
    uint32_t struct_size;   /* sizeof(uscale_upscale_info) */
    VkCommandBuffer command_buffer;
    uscale_image color;     /* render-size colour, without pre-exposure */
    uscale_image depth;     /* render-size depth */
    /* Render-size R16G16_SFLOAT: unjittered previous UV minus current UV,
     * normalized to the active render extent; +X right, +Y down. */
    uscale_image motion;
    uscale_image output;    /* display-size output, storage usage */
    uscale_extent render_extent;
    /* Camera jitter in render pixels, +X right, +Y down, as returned by
     * FSR's jitter helper (before conversion to projection-matrix offsets). */
    float jitter_x;
    float jitter_y;
    float frame_time_ms;
    float sharpness;        /* 0..1 */
    float camera_near;      /* camera distances in metres */
    float camera_far;
    float camera_fov_y;     /* radians */
    uint32_t reset_history; /* camera cut: 1 or 0 */
} uscale_upscale_info;

typedef struct uscale_framegen_info {
    uint32_t struct_size;   /* sizeof(uscale_framegen_info) */
    VkCommandBuffer command_buffer;
    uscale_image present_color; /* the real frame at display size */
    uscale_image generated;     /* where the generated frame goes; distinct image */
    uint64_t frame_id;          /* increases by 1 each real frame */
    float frame_time_ms;
    uint32_t reset;             /* camera cut: 1 or 0 */
} uscale_framegen_info;

/* Fills caps from the device and the runtimes found on this machine. */
uscale_result uscale_query_caps(VkPhysicalDevice physical_device,
                                PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                                VkInstance instance, const char *fsr4_asset_dir,
                                uscale_device_caps *out_caps);

uscale_result uscale_context_create(const uscale_context_create_info *info,
                                    uscale_context **out_context);

/* The backend the context runs, and why it was chosen. */
uscale_backend uscale_context_backend(const uscale_context *context, const char **out_reason);

uscale_result uscale_record_upscale(uscale_context *context, const uscale_upscale_info *info);

uscale_result uscale_record_framegen(uscale_context *context, const uscale_framegen_info *info);

/* Call once the game's fence for frame_id has signalled. */
uscale_result uscale_retire_frame(uscale_context *context, uint64_t frame_id);

void uscale_context_destroy(uscale_context *context);

#ifdef __cplusplus
}
#endif

#endif /* USCALE_VULKAN_H */
