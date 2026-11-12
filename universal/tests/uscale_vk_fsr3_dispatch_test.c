/* Pure FSR 3 dispatch contract: fake handles are never dereferenced.
 * Runs with Vulkan headers, including USCALE_WITH_FFX=OFF; no GPU required.
 * SPDX-License-Identifier: MIT */
#include "backend_fsr3.h"
#include "uscale_test.h"

static void check_image(const FfxVkPortableImage *got, VkImage image, VkImageView view,
                         VkFormat format, uint32_t width, uint32_t height,
                         VkImageUsageFlags usage, VkImageAspectFlags aspect,
                         FfxVkPortableResourceState state)
{
    CHECK_INT(got->structSize, sizeof(*got));
    CHECK(got->image == image);
    CHECK(got->view == view);
    CHECK_INT(got->format, format);
    CHECK_INT(got->extent.width, width);
    CHECK_INT(got->extent.height, height);
    CHECK_INT(got->mipCount, 1);
    CHECK_INT(got->arrayLayers, 1);
    CHECK_INT(got->usage, usage);
    CHECK_INT(got->aspect, aspect);
    CHECK_INT(got->state, state);
}

static void check_absent(const FfxVkPortableImage *got)
{
    CHECK_INT(got->structSize, 0);
    CHECK(got->image == VK_NULL_HANDLE);
    CHECK(got->view == VK_NULL_HANDLE);
    CHECK_INT(got->format, VK_FORMAT_UNDEFINED);
    CHECK_INT(got->extent.width, 0);
    CHECK_INT(got->extent.height, 0);
    CHECK_INT(got->mipCount, 0);
    CHECK_INT(got->arrayLayers, 0);
    CHECK_INT(got->usage, 0);
    CHECK_INT(got->aspect, 0);
    CHECK_INT(got->state, FFX_VK_PORTABLE_RESOURCE_STATE_UNDEFINED);
}

int main(void)
{
    uscale_upscale_info info = {0};
    FfxVkPortableUpscaleDispatchInfo dispatch;
    unsigned char before[sizeof(dispatch)];
    const struct {
        VkImageLayout layout;
        FfxVkPortableResourceState state;
    } layouts[] = {
        {VK_IMAGE_LAYOUT_UNDEFINED, FFX_VK_PORTABLE_RESOURCE_STATE_UNDEFINED},
        {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, FFX_VK_PORTABLE_RESOURCE_STATE_COMPUTE_READ},
        {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, FFX_VK_PORTABLE_RESOURCE_STATE_COLOR_ATTACHMENT},
        {VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, FFX_VK_PORTABLE_RESOURCE_STATE_DEPTH_ATTACHMENT},
        {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, FFX_VK_PORTABLE_RESOURCE_STATE_TRANSFER_SOURCE},
        {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, FFX_VK_PORTABLE_RESOURCE_STATE_TRANSFER_DESTINATION},
        {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, FFX_VK_PORTABLE_RESOURCE_STATE_PRESENT}
    };
    uscale_image *images[] = {&info.color, &info.depth, &info.motion, &info.output};
    size_t i;
    size_t j;

    info.struct_size = sizeof(info);
    info.command_buffer = (VkCommandBuffer)(uintptr_t)0x10;
    info.color = (uscale_image){(VkImage)(uintptr_t)0x21, (VkImageView)(uintptr_t)0x31,
        VK_FORMAT_R16G16B16A16_SFLOAT, {1600, 1000}, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    info.depth = (uscale_image){(VkImage)(uintptr_t)0x22, (VkImageView)(uintptr_t)0x32,
        VK_FORMAT_D32_SFLOAT, {1536, 960}, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    info.motion = (uscale_image){(VkImage)(uintptr_t)0x23, VK_NULL_HANDLE,
        VK_FORMAT_R16G16_SFLOAT, {1440, 900}, VK_IMAGE_LAYOUT_GENERAL};
    info.output = (uscale_image){(VkImage)(uintptr_t)0x24, (VkImageView)(uintptr_t)0x34,
        VK_FORMAT_R8G8B8A8_UNORM, {2560, 1440}, VK_IMAGE_LAYOUT_GENERAL};
    info.render_extent = (uscale_extent){1280, 720};
    info.jitter_x = -0.375f;
    info.jitter_y = 0.125f;
    info.frame_time_ms = 12.5f;
    info.sharpness = 0.625f;
    info.camera_near = 0.25f;
    info.camera_far = 4096.0f;
    info.camera_fov_y = 1.25f;
    info.reset_history = 7; /* Any nonzero input becomes VK_TRUE. */
    memset(&dispatch, 0xa5, sizeof(dispatch));
    CHECK_INT(uscale_fsr3_map_dispatch(&info, UINT64_C(0x123456789), &dispatch), USCALE_OK);

    CHECK_INT(dispatch.structSize, sizeof(dispatch));
    CHECK(dispatch.commandBuffer == (VkCommandBuffer)(uintptr_t)0x10);
    check_image(&dispatch.color, (VkImage)(uintptr_t)0x21, (VkImageView)(uintptr_t)0x31,
        VK_FORMAT_R16G16B16A16_SFLOAT, 1600, 1000, VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT, FFX_VK_PORTABLE_RESOURCE_STATE_COMPUTE_READ);
    check_image(&dispatch.depth, (VkImage)(uintptr_t)0x22, (VkImageView)(uintptr_t)0x32,
        VK_FORMAT_D32_SFLOAT, 1536, 960, VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT, FFX_VK_PORTABLE_RESOURCE_STATE_DEPTH_ATTACHMENT);
    check_image(&dispatch.motionVectors, (VkImage)(uintptr_t)0x23, VK_NULL_HANDLE,
        VK_FORMAT_R16G16_SFLOAT, 1440, 900, VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT, FFX_VK_PORTABLE_RESOURCE_STATE_GENERIC_READ);
    check_image(&dispatch.output, (VkImage)(uintptr_t)0x24, (VkImageView)(uintptr_t)0x34,
        VK_FORMAT_R8G8B8A8_UNORM, 2560, 1440, VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT, FFX_VK_PORTABLE_RESOURCE_STATE_UNORDERED_ACCESS);
    check_absent(&dispatch.exposure);
    check_absent(&dispatch.reactiveMask);
    check_absent(&dispatch.transparencyAndCompositionMask);
    CHECK(dispatch.jitterOffset.x == -0.375f);
    CHECK(dispatch.jitterOffset.y == 0.125f);
    CHECK(dispatch.motionVectorScale.x == 1280.0f);
    CHECK(dispatch.motionVectorScale.y == 720.0f);
    CHECK_INT(dispatch.renderSize.width, 1280);
    CHECK_INT(dispatch.renderSize.height, 720);
    CHECK_INT(dispatch.outputSize.width, 2560);
    CHECK_INT(dispatch.outputSize.height, 1440);
    CHECK(dispatch.frameTimeMilliseconds == 12.5f);
    CHECK(dispatch.preExposure == 1.0f);
    CHECK(dispatch.cameraNear == 0.25f);
    CHECK(dispatch.cameraFar == 4096.0f);
    CHECK(dispatch.cameraVerticalFovRadians == 1.25f);
    CHECK(dispatch.viewSpaceToMeters == 1.0f);
    CHECK(dispatch.sharpness == 0.625f);
    CHECK_INT(dispatch.enableSharpening, VK_TRUE);
    CHECK_INT(dispatch.reset, VK_TRUE);
    CHECK_INT(dispatch.frameId, UINT64_C(0x123456789));

    /* Reuse dirty output: no stale reset, sharpening, jitter or scale. */
    info.render_extent = (uscale_extent){960, 540};
    info.jitter_x = 0.25f;
    info.jitter_y = -0.5f;
    info.sharpness = 0.0f;
    info.reset_history = 0;
    CHECK_INT(uscale_fsr3_map_dispatch(&info, 0, &dispatch), USCALE_OK);
    CHECK(dispatch.jitterOffset.x == 0.25f);
    CHECK(dispatch.jitterOffset.y == -0.5f);
    CHECK(dispatch.motionVectorScale.x == 960.0f);
    CHECK(dispatch.motionVectorScale.y == 540.0f);
    CHECK_INT(dispatch.renderSize.width, 960);
    CHECK_INT(dispatch.renderSize.height, 540);
    CHECK(dispatch.sharpness == 0.0f);
    CHECK_INT(dispatch.enableSharpening, VK_FALSE);
    CHECK_INT(dispatch.reset, VK_FALSE);
    CHECK_INT(dispatch.frameId, 0);

    for (i = 0; i < sizeof(layouts) / sizeof(layouts[0]); ++i) {
        for (j = 0; j < sizeof(images) / sizeof(images[0]); ++j) {
            images[j]->layout = layouts[i].layout;
        }
        CHECK_INT(uscale_fsr3_map_dispatch(&info, 1, &dispatch), USCALE_OK);
        CHECK_INT(dispatch.color.state, layouts[i].state);
        CHECK_INT(dispatch.depth.state, layouts[i].state);
        CHECK_INT(dispatch.motionVectors.state, layouts[i].state);
        CHECK_INT(dispatch.output.state, layouts[i].state);
    }
    memcpy(before, &dispatch, sizeof(dispatch));
    for (j = 0; j < sizeof(images) / sizeof(images[0]); ++j) {
        images[j]->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        CHECK_INT(uscale_fsr3_map_dispatch(&info, 1, &dispatch), USCALE_ERROR_UNSUPPORTED);
        CHECK(memcmp(before, &dispatch, sizeof(dispatch)) == 0);
        images[j]->layout = VK_IMAGE_LAYOUT_GENERAL;
    }
    CHECK_INT(uscale_fsr3_map_dispatch(NULL, 1, &dispatch), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK(memcmp(before, &dispatch, sizeof(dispatch)) == 0);
    CHECK_INT(uscale_fsr3_map_dispatch(&info, 1, NULL), USCALE_ERROR_INVALID_ARGUMENT);
    info.struct_size = 0;
    CHECK_INT(uscale_fsr3_map_dispatch(&info, 1, &dispatch), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK(memcmp(before, &dispatch, sizeof(dispatch)) == 0);
    USCALE_TEST_END;
}
