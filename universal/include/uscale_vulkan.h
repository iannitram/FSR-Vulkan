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
    /* Added in 0.2. Colour format of the real frame and the generated frame:
     * VK_FORMAT_R8G8B8A8_UNORM or VK_FORMAT_R16G16B16A16_SFLOAT. 0
     * (VK_FORMAT_UNDEFINED) means no frame generation; any other format with
     * enable_frame_generation set is USCALE_ERROR_UNSUPPORTED. Read only when
     * struct_size covers it and api_version is 0.2 or later (on 64-bit ABIs
     * this field sits in 0.1's tail padding, so the size alone cannot tell). */
    VkFormat framegen_color_format;
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

/* Frame generation (FSR 3.1.6, SPEC M5): two calls per real frame, then the
 * game records, submits and presents the returned plan. The library never
 * acquires, presents or owns the swapchain.
 *
 * Device: frame generation needs what FSR 3 needs (above). Upstream's
 * frame-generation shaders also use compute shader derivatives: when the
 * device offers VK_KHR_compute_shader_derivatives, enable it with its
 * computeDerivativeGroupLinear feature.
 *
 * Image rules: the inputs are sampled (VK_IMAGE_USAGE_SAMPLED_BIT), and
 * present_color is also a transfer source (upstream copies it into its
 * history); the generated image is sampled and storage. present_color and generated use
 * the context's framegen_color_format at display size. Frame-generation depth
 * is a sampled VK_FORMAT_R32_SFLOAT colour image (not a D32 depth image).
 * Every float must be finite; frame_time_ms, camera_near, camera_fov_y and
 * view_space_to_meters must be > 0, camera_far > camera_near and
 * 0 <= min_luminance <= max_luminance. A field outside these rules is
 * USCALE_ERROR_INVALID_ARGUMENT and nothing is recorded. */

/* The values of upstream's FrameGenerationTransferFunction. */
typedef enum uscale_transfer_function {
    USCALE_TRANSFER_SRGB = 0,
    USCALE_TRANSFER_PQ = 1,
    USCALE_TRANSFER_SCRGB = 2
} uscale_transfer_function;

/* Step 1, recorded every real frame after the scene is rendered and before
 * the dispatch for the same frame_id. Runs optical flow on the real frame. */
typedef struct uscale_framegen_prepare_info {
    uint32_t struct_size;          /* sizeof(uscale_framegen_prepare_info) */
    VkCommandBuffer command_buffer;
    uscale_image present_color;    /* the real frame at display size; sampled, transfer source */
    uscale_image depth;            /* render-size R32_SFLOAT, sampled */
    uscale_image motion;           /* render-size R16G16_SFLOAT, same convention as uscale_upscale_info */
    uscale_extent render_extent;
    float jitter_x, jitter_y;      /* render pixels, as in uscale_upscale_info */
    float frame_time_ms;
    float camera_near, camera_far; /* metres, > 0 */
    float camera_fov_y;            /* radians, > 0 */
    float view_space_to_meters;    /* > 0; 1.0 when view space is in metres */
    float camera_position[3], camera_up[3], camera_right[3], camera_forward[3];
    float min_luminance, max_luminance; /* 0 <= min <= max */
    uscale_transfer_function transfer;
    uint64_t frame_id;             /* > 0 and greater than the previous prepare's; 1 more each real frame */
    uint32_t reset;                /* camera cut: 1 or 0 */
} uscale_framegen_prepare_info;

/* The game owns the swapchain: it acquires two images (generated target, then
 * real target) and passes the outcome here. paired = 0: only the first image
 * was acquired (or the two do not form a pair). Flags are 1 or 0. */
typedef struct uscale_acquired_images {
    uint32_t generated_index, real_index;
    VkSemaphore generated_available, real_available;
    uint32_t generated_acquired, real_acquired, paired;
} uscale_acquired_images;

/* Step 2, after a successful prepare for the same frame_id. Reuses that
 * prepare's camera near/far/FOV, view_space_to_meters, luminance and transfer. */
typedef struct uscale_framegen_dispatch_info {
    uint32_t struct_size;          /* sizeof(uscale_framegen_dispatch_info) */
    VkCommandBuffer command_buffer;
    uscale_image present_color;    /* the same image as prepare's present_color */
    uscale_image generated;        /* output; distinct from present_color and prepare's inputs */
    uscale_image distortion;       /* optional R16G16_SFLOAT; image == VK_NULL_HANDLE for none */
    float frame_time_ms;
    uint64_t frame_id;             /* must equal the last prepare's */
    uint32_t reset;                /* 1 or 0; a reset prepare also counts as a reset */
    uscale_acquired_images acquired;
} uscale_framegen_dispatch_info;

typedef struct uscale_present_slot {
    uint32_t image_index;
    VkSemaphore image_available;
    uint32_t use_generated;        /* 1: present the generated scene in this slot */
} uscale_present_slot;

/* What the game records, submits and presents, in slot order. With two slots
 * the order is always generated target then real target; slot 0 may still
 * show the real scene (reset or no interpolation) and must still be presented
 * so its acquired image is released. */
typedef struct uscale_present_plan {
    uscale_present_slot slots[2];
    uint32_t slot_count;           /* 1: present normally; 2: generated then real */
} uscale_present_plan;

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

/* Frame generation step 1. USCALE_ERROR_UNSUPPORTED when the context has no
 * frame generation (built without USCALE_WITH_FFX, or not requested). */
uscale_result uscale_record_framegen_prepare(uscale_context *context,
                                             const uscale_framegen_prepare_info *info);

/* Frame generation step 2: records the interpolated frame into `generated`
 * and fills *out_plan from upstream's ffxVkFrameGenerationBuildPresentPlan.
 * A dispatch without its own prepare, or an acquire outcome that cannot be
 * planned (for example duplicate indices: recreate the swapchain), is
 * USCALE_ERROR_INVALID_ARGUMENT, records nothing and leaves slot_count 0.
 * When upstream fails after recording started, the error is returned and
 * *out_plan is still filled (slot 0 real) so the acquired images can be
 * presented. A prepare without a successful dispatch must be retired with
 * uscale_retire_frame (after its fence) before the next prepare. */
uscale_result uscale_record_framegen_dispatch(uscale_context *context,
                                              const uscale_framegen_dispatch_info *info,
                                              uscale_present_plan *out_plan);

/* Call once the game's fence for frame_id has signalled. */
uscale_result uscale_retire_frame(uscale_context *context, uint64_t frame_id);

void uscale_context_destroy(uscale_context *context);

#ifdef __cplusplus
}
#endif

#endif /* USCALE_VULKAN_H */
