/* Backend FSR3: upstream FSR 3.1.x upscaler (ffx-vulkan::fsr3-vk-backend-1.1.4).
 * Pure dispatch mapping; GPU lifecycle remains a stub. SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "backend.h"
#include "backend_fsr3.h"

static uscale_result fsr3_map_image(const uscale_image *image,
                                    VkImageUsageFlags usage, VkImageAspectFlags aspect,
                                    FfxVkPortableImage *out)
{
    FfxVkPortableImage mapped = {0};
    mapped.structSize = sizeof(mapped);
    mapped.image = image->image;
    mapped.view = image->view;
    mapped.format = image->format;
    mapped.extent.width = image->extent.width;
    mapped.extent.height = image->extent.height;
    mapped.mipCount = 1;
    mapped.arrayLayers = 1;
    mapped.usage = usage;
    mapped.aspect = aspect;

    /* Match the layouts produced by upstream's getVKImageLayoutFromResourceState.
     * GENERAL uses GENERIC_READ for inputs, UNORDERED_ACCESS for output. */
    switch (image->layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        mapped.state = FFX_VK_PORTABLE_RESOURCE_STATE_UNDEFINED;
        break;
    case VK_IMAGE_LAYOUT_GENERAL:
        mapped.state = (usage & VK_IMAGE_USAGE_STORAGE_BIT)
            ? FFX_VK_PORTABLE_RESOURCE_STATE_UNORDERED_ACCESS
            : FFX_VK_PORTABLE_RESOURCE_STATE_GENERIC_READ;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        mapped.state = FFX_VK_PORTABLE_RESOURCE_STATE_COMPUTE_READ;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        mapped.state = FFX_VK_PORTABLE_RESOURCE_STATE_COLOR_ATTACHMENT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        mapped.state = FFX_VK_PORTABLE_RESOURCE_STATE_DEPTH_ATTACHMENT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        mapped.state = FFX_VK_PORTABLE_RESOURCE_STATE_TRANSFER_SOURCE;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        mapped.state = FFX_VK_PORTABLE_RESOURCE_STATE_TRANSFER_DESTINATION;
        break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        mapped.state = FFX_VK_PORTABLE_RESOURCE_STATE_PRESENT;
        break;
    default:
        /* No portable state represents e.g. DEPTH_STENCIL_READ_ONLY_OPTIMAL. */
        return USCALE_ERROR_UNSUPPORTED;
    }
    *out = mapped;
    return USCALE_OK;
}

uscale_result uscale_fsr3_map_dispatch(const uscale_upscale_info *info,
                                      uint64_t frame_id,
                                      FfxVkPortableUpscaleDispatchInfo *out_dispatch)
{
    FfxVkPortableUpscaleDispatchInfo mapped = {0};
    if (info == NULL || out_dispatch == NULL || info->struct_size < sizeof(*info)) {
        return USCALE_ERROR_INVALID_ARGUMENT;
    }
    if (fsr3_map_image(&info->color, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                       &mapped.color) != USCALE_OK ||
        fsr3_map_image(&info->depth, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_DEPTH_BIT,
                       &mapped.depth) != USCALE_OK ||
        fsr3_map_image(&info->motion, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                       &mapped.motionVectors) != USCALE_OK ||
        fsr3_map_image(&info->output, VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                       &mapped.output) != USCALE_OK) {
        return USCALE_ERROR_UNSUPPORTED;
    }
    mapped.structSize = sizeof(mapped);
    mapped.commandBuffer = info->command_buffer;
    /* Both APIs use FSR's render-pixel camera jitter, not projection offsets.
     * Preserve both signs: upstream forwards jitterOffset to the SDK, whose
     * upsample shader subtracts Jitter() to find the unjittered sample. */
    mapped.jitterOffset.x = info->jitter_x;
    mapped.jitterOffset.y = info->jitter_y;
    /* uscale supplies unjittered previous-minus-current UV motion. FSR expects
     * a scale to pixels, then divides by renderSize internally. Use the active
     * render extent, not the image allocation or display size. */
    mapped.motionVectorScale.x = (float)info->render_extent.width;
    mapped.motionVectorScale.y = (float)info->render_extent.height;
    mapped.renderSize.width = info->render_extent.width;
    mapped.renderSize.height = info->render_extent.height;
    mapped.outputSize.width = info->output.extent.width;
    mapped.outputSize.height = info->output.extent.height;
    mapped.frameTimeMilliseconds = info->frame_time_ms; /* SDK also takes ms. */
    mapped.cameraNear = info->camera_near;
    mapped.cameraFar = info->camera_far;
    mapped.cameraVerticalFovRadians = info->camera_fov_y;
    mapped.sharpness = info->sharpness;
    mapped.enableSharpening = info->sharpness > 0.0f ? VK_TRUE : VK_FALSE;
    mapped.reset = info->reset_history != 0 ? VK_TRUE : VK_FALSE;
    mapped.frameId = frame_id;
    /* API has no exposure/mask inputs: zero descriptors mean absent to FSR.
     * Neutral pre-exposure assumes un-pre-exposed colour; camera units are
     * metres, so view-space conversion is identity. No camera defaults. */
    mapped.preExposure = 1.0f;
    mapped.viewSpaceToMeters = 1.0f;
    *out_dispatch = mapped;
    return USCALE_OK;
}

static uscale_result fsr3_create(const uscale_context_create_info *info, void **out_state)
{
    (void)info;
    if (out_state != NULL) {
        *out_state = NULL;
    }
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

static uscale_result fsr3_record_upscale(void *state, const uscale_upscale_info *info)
{
    (void)state;
    (void)info;
    return USCALE_ERROR_NOT_IMPLEMENTED;
}

static void fsr3_destroy(void *state)
{
    (void)state;
}

static const uscale_backend_ops fsr3_ops = {
    USCALE_BACKEND_FSR3, fsr3_create, fsr3_record_upscale, fsr3_destroy
};

const uscale_backend_ops *uscale_backend_fsr3_ops(void)
{
    return &fsr3_ops;
}
