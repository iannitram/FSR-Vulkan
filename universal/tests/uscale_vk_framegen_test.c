/* Frame generation through the context (SPEC M5, #13) without a GPU: fake
 * upscaler backend and caps; with USCALE_WITH_FFX, upstream's FSR 3.1.6 calls
 * are replaced by fakes, while the present plan comes from upstream's real
 * ffxVkFrameGenerationBuildPresentPlan. Fake handles are never dereferenced.
 * SPDX-License-Identifier: MIT */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "backend.h"
#include "uscale_test.h"

#ifdef USCALE_HAVE_FFX
#include "ffx_vk_framegeneration_presenter.h"
#endif

#define FAKE(type, n) ((type)(uintptr_t)(n))

static int g_live;

static uscale_result fake_create(const uscale_context_create_info *info, void **out_state)
{
    (void)info;
    *out_state = malloc(1);
    g_live++;
    return USCALE_OK;
}
static uscale_result fake_upscale(void *state, const uscale_upscale_info *info)
{
    (void)state;
    (void)info;
    return USCALE_OK;
}
static void fake_destroy(void *state)
{
    g_live--;
    free(state);
}
static const uscale_backend_ops k_fsr3 = {USCALE_BACKEND_FSR3, fake_create, fake_upscale,
                                          fake_destroy};
static const uscale_backend_ops *fake_ops_for(uscale_backend id)
{
    return id == USCALE_BACKEND_FSR3 ? &k_fsr3 : NULL;
}
static uscale_result fake_caps(const uscale_context_create_info *info, uscale_device_caps *out)
{
    uscale_device_caps caps;
    (void)info;
    memset(&caps, 0, sizeof(caps));
    caps.struct_size = sizeof(caps);
    *out = caps;
    return USCALE_OK;
}
/* framegen NULL: the real framegen_fsr3.c. */
static const uscale_context_hooks k_hooks = {fake_ops_for, fake_caps, NULL};

static uscale_context_create_info make_info(void)
{
    uscale_context_create_info info;
    memset(&info, 0, sizeof(info));
    info.struct_size = sizeof(info);
    info.api_version = USCALE_VERSION;
    info.physical_device = FAKE(VkPhysicalDevice, 0x10);
    info.device = FAKE(VkDevice, 0x20);
    info.preferred_backend = USCALE_BACKEND_FSR3;
    info.max_render_extent.width = 64;
    info.max_render_extent.height = 64;
    info.display_extent.width = 128;
    info.display_extent.height = 128;
    info.enable_frame_generation = 1;
    info.framegen_color_format = VK_FORMAT_R8G8B8A8_UNORM;
    return info;
}

static uscale_image image(uintptr_t handle, VkFormat format, uint32_t size, VkImageLayout layout)
{
    uscale_image im;
    memset(&im, 0, sizeof(im));
    im.image = FAKE(VkImage, handle);
    im.format = format;
    im.extent.width = size;
    im.extent.height = size;
    im.layout = layout;
    return im;
}

static uscale_framegen_prepare_info make_prepare(uint64_t frame_id, uint32_t reset)
{
    uscale_framegen_prepare_info p;
    memset(&p, 0, sizeof(p));
    p.struct_size = sizeof(p);
    p.command_buffer = FAKE(VkCommandBuffer, 0x30);
    p.present_color = image(0x100, VK_FORMAT_R8G8B8A8_UNORM, 128,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    p.depth = image(0x101, VK_FORMAT_R32_SFLOAT, 64, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    p.motion = image(0x102, VK_FORMAT_R16G16_SFLOAT, 64, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    p.render_extent.width = 60;
    p.render_extent.height = 50;
    p.jitter_x = 0.25f;
    p.jitter_y = -0.25f;
    p.frame_time_ms = 16.6667f;
    p.camera_near = 0.1f;
    p.camera_far = 1000.0f;
    p.camera_fov_y = 1.0471975512f;
    p.view_space_to_meters = 1.0f;
    p.camera_up[1] = 1.0f;
    p.camera_right[0] = 1.0f;
    p.camera_forward[2] = -1.0f;
    p.min_luminance = 0.0f;
    p.max_luminance = 1.0f;
    p.transfer = USCALE_TRANSFER_SRGB;
    p.frame_id = frame_id;
    p.reset = reset;
    return p;
}

static uscale_framegen_dispatch_info make_dispatch(uint64_t frame_id, uint32_t reset)
{
    uscale_framegen_dispatch_info d;
    memset(&d, 0, sizeof(d));
    d.struct_size = sizeof(d);
    d.command_buffer = FAKE(VkCommandBuffer, 0x30);
    d.present_color = image(0x100, VK_FORMAT_R8G8B8A8_UNORM, 128,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    d.generated = image(0x103, VK_FORMAT_R8G8B8A8_UNORM, 128, VK_IMAGE_LAYOUT_GENERAL);
    d.frame_time_ms = 16.6667f;
    d.frame_id = frame_id;
    d.reset = reset;
    d.acquired.generated_index = 2;
    d.acquired.real_index = 0;
    d.acquired.generated_available = FAKE(VkSemaphore, 0x200);
    d.acquired.real_available = FAKE(VkSemaphore, 0x201);
    d.acquired.generated_acquired = 1;
    d.acquired.real_acquired = 1;
    d.acquired.paired = 1;
    return d;
}

/* PM condition: a caller built against 0.1 (struct_size before
 * framegen_color_format) still gets a working context, with no frame
 * generation, and nothing past its size is read. */
static void test_old_struct_size(void)
{
    unsigned char *raw = (unsigned char *)malloc(sizeof(uscale_context_create_info) + 16);
    uscale_context_create_info info = make_info();
    uscale_framegen_prepare_info p = make_prepare(1, 1);
    uscale_framegen_dispatch_info d = make_dispatch(1, 1);
    uscale_present_plan plan;
    uscale_context *ctx = NULL;
    VkFormat bogus = VK_FORMAT_D16_UNORM; /* would be UNSUPPORTED if read */

    info.struct_size = (uint32_t)offsetof(uscale_context_create_info, framegen_color_format);
    info.api_version = USCALE_MAKE_VERSION(0u, 1u, 0u);
    CHECK(raw != NULL);
    if (raw == NULL)
        return;
    memset(raw, 0xA5, sizeof(uscale_context_create_info) + 16);
    memcpy(raw, &info, info.struct_size);
    memcpy(raw + info.struct_size, &bogus, sizeof(bogus));
    CHECK_INT(uscale_context_create((const uscale_context_create_info *)(void *)raw, &ctx),
              USCALE_OK);
    CHECK(ctx != NULL);
    CHECK_INT(uscale_record_framegen_prepare(ctx, &p), USCALE_ERROR_UNSUPPORTED);
    CHECK_INT(uscale_record_framegen_dispatch(ctx, &d, &plan), USCALE_ERROR_UNSUPPORTED);
    CHECK_INT(plan.slot_count, 0);
    CHECK_INT(uscale_retire_frame(ctx, 1), USCALE_OK);
    uscale_context_destroy(ctx);
    free(raw);

    /* Full size but a 0.1 api_version: on 64-bit the field is 0.1's padding. */
    info = make_info();
    info.api_version = USCALE_MAKE_VERSION(0u, 1u, 0u);
    info.framegen_color_format = bogus;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(uscale_record_framegen_prepare(ctx, &p), USCALE_ERROR_UNSUPPORTED);
    uscale_context_destroy(ctx);

    /* Smaller than 0.1's struct: still a version mismatch. */
    info = make_info();
    info.struct_size = (uint32_t)offsetof(uscale_context_create_info, hdr);
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_ERROR_VERSION_MISMATCH);
    CHECK_INT(g_live, 0);
}

static void test_create_format(void)
{
    uscale_context_create_info info = make_info();
    uscale_framegen_prepare_info p = make_prepare(1, 1);
    uscale_context *ctx = NULL;

    info.framegen_color_format = VK_FORMAT_B8G8R8A8_UNORM;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_ERROR_UNSUPPORTED);
    CHECK(ctx == NULL);
    /* 0 means no frame generation, even with enable_frame_generation set. */
    info.framegen_color_format = VK_FORMAT_UNDEFINED;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(uscale_record_framegen_prepare(ctx, &p), USCALE_ERROR_UNSUPPORTED);
    uscale_context_destroy(ctx);
    CHECK_INT(g_live, 0);
}

#ifndef USCALE_HAVE_FFX
/* Built without USCALE_WITH_FFX: a context asking for frame generation is
 * created, and both calls are unsupported. */
static void test_no_ffx(void)
{
    uscale_context_create_info info = make_info();
    uscale_framegen_prepare_info p = make_prepare(1, 1);
    uscale_framegen_dispatch_info d = make_dispatch(1, 1);
    uscale_present_plan plan;
    uscale_context *ctx = NULL;

    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(uscale_record_framegen_prepare(ctx, &p), USCALE_ERROR_UNSUPPORTED);
    CHECK_INT(uscale_record_framegen_dispatch(ctx, &d, &plan), USCALE_ERROR_UNSUPPORTED);
    CHECK_INT(plan.slot_count, 0);
    CHECK_INT(uscale_retire_frame(ctx, 1), USCALE_OK);
    uscale_context_destroy(ctx);
    CHECK_INT(g_live, 0);
}
#else
static int g_up_prepares, g_up_dispatches, g_up_live;
static uint64_t g_up_retired;
static FfxVkFsr3_3_1_6FrameGenerationResult g_up_dispatch_result;
static FfxVkFsr3_3_1_6FrameGenerationCreateInfo g_up_create;
static FfxVkFsr3_3_1_6FrameGenerationPrepareInfo g_up_prepare;
static FfxVkFsr3_3_1_6FrameGenerationDispatchInfo g_up_dispatch;
static int g_up_context;

static FfxVkFsr3_3_1_6FrameGenerationResult up_create(
    const FfxVkFsr3_3_1_6FrameGenerationCreateInfo *info,
    FfxVkFsr3_3_1_6FrameGenerationContext **out)
{
    g_up_create = *info;
    *out = (FfxVkFsr3_3_1_6FrameGenerationContext *)(void *)&g_up_context;
    g_up_live++;
    return FFX_VK_FSR3_3_1_6_FRAMEGEN_OK;
}
static FfxVkFsr3_3_1_6FrameGenerationResult up_prepare(
    FfxVkFsr3_3_1_6FrameGenerationContext *c, const FfxVkFsr3_3_1_6FrameGenerationPrepareInfo *i)
{
    (void)c;
    g_up_prepare = *i;
    g_up_prepares++;
    return FFX_VK_FSR3_3_1_6_FRAMEGEN_OK;
}
static FfxVkFsr3_3_1_6FrameGenerationResult up_dispatch(
    FfxVkFsr3_3_1_6FrameGenerationContext *c, const FfxVkFsr3_3_1_6FrameGenerationDispatchInfo *i)
{
    (void)c;
    g_up_dispatch = *i;
    g_up_dispatches++;
    return g_up_dispatch_result;
}
static FfxVkFsr3_3_1_6FrameGenerationResult up_retire(FfxVkFsr3_3_1_6FrameGenerationContext *c,
                                                      uint64_t id)
{
    (void)c;
    g_up_retired = id;
    return FFX_VK_FSR3_3_1_6_FRAMEGEN_OK;
}
static void up_destroy(FfxVkFsr3_3_1_6FrameGenerationContext *c)
{
    (void)c;
    g_up_live--;
}
static const uscale_fsr3fg_upstream k_up = {up_create, up_prepare, up_dispatch, up_retire,
                                            up_destroy};

/* Upstream's own plan for the same acquire outcome. */
static FfxVkFrameGenerationPresentPlan upstream_plan(const uscale_acquired_images *a,
                                                     bool dispatched, bool reset, bool *ok)
{
    FfxVkFrameGenerationAcquiredPair pair;
    FfxVkFrameGenerationPresentPlan plan;
    memset(&pair, 0, sizeof(pair));
    pair.generatedImageIndex = a->generated_index;
    pair.realImageIndex = a->real_index;
    pair.generatedAvailableSemaphore = a->generated_available;
    pair.realAvailableSemaphore = a->real_available;
    pair.generatedImageAcquired = a->generated_acquired != 0;
    pair.realImageAcquired = a->real_acquired != 0;
    pair.paired = a->paired != 0;
    *ok = ffxVkFrameGenerationBuildPresentPlan(&pair, dispatched, reset, &plan);
    return plan;
}

static void check_same_plan(const uscale_present_plan *got, const uscale_acquired_images *a,
                            bool dispatched, bool reset)
{
    bool ok = false;
    FfxVkFrameGenerationPresentPlan want = upstream_plan(a, dispatched, reset, &ok);
    uint32_t i;
    CHECK(ok);
    CHECK_INT(got->slot_count, want.slotCount);
    for (i = 0; i < want.slotCount && i < 2; i++) {
        CHECK_INT(got->slots[i].image_index, want.slots[i].imageIndex);
        CHECK(got->slots[i].image_available == want.slots[i].imageAvailableSemaphore);
        CHECK_INT(got->slots[i].use_generated, want.slots[i].useInterpolatedScene ? 1 : 0);
    }
}

/* Every invalid prepare: INVALID_ARGUMENT, upstream never called. */
static void check_bad_prepare(uscale_context *ctx, const uscale_framegen_prepare_info *p)
{
    int before = g_up_prepares;
    CHECK_INT(uscale_record_framegen_prepare(ctx, p), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(g_up_prepares, before);
}

static void check_bad_dispatch(uscale_context *ctx, const uscale_framegen_dispatch_info *d)
{
    uscale_present_plan plan;
    int before = g_up_dispatches;
    plan.slot_count = 7;
    CHECK_INT(uscale_record_framegen_dispatch(ctx, d, &plan), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(g_up_dispatches, before);
    CHECK_INT(plan.slot_count, 0);
}

static void test_ffx(void)
{
    uscale_context_create_info info = make_info();
    uscale_framegen_prepare_info p, bad;
    uscale_framegen_dispatch_info d, badd;
    uscale_present_plan plan;
    uscale_context *ctx = NULL;
    const float nan = (float)NAN;
    const float inf = (float)INFINITY;

    uscale_framegen_fsr3_set_upstream(&k_up);
    g_up_dispatch_result = FFX_VK_FSR3_3_1_6_FRAMEGEN_OK;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(g_up_live, 1);
    CHECK_INT(g_up_create.colorFormat, VK_FORMAT_R8G8B8A8_UNORM);
    CHECK_INT(g_up_create.displayWidth, 128);
    CHECK_INT(g_up_create.maxRenderWidth, 64);

    /* A dispatch without a prepare. */
    d = make_dispatch(1, 1);
    check_bad_dispatch(ctx, &d);

    /* Non-finite floats and the bridge's range rules: nothing recorded. */
    p = make_prepare(1, 1);
    bad = p; bad.jitter_x = nan; check_bad_prepare(ctx, &bad);
    bad = p; bad.jitter_y = inf; check_bad_prepare(ctx, &bad);
    bad = p; bad.frame_time_ms = 0.0f; check_bad_prepare(ctx, &bad);
    bad = p; bad.camera_near = nan; check_bad_prepare(ctx, &bad);
    bad = p; bad.camera_far = inf; check_bad_prepare(ctx, &bad);
    bad = p; bad.camera_far = bad.camera_near; check_bad_prepare(ctx, &bad);
    bad = p; bad.camera_fov_y = -1.0f; check_bad_prepare(ctx, &bad);
    bad = p; bad.view_space_to_meters = 0.0f; check_bad_prepare(ctx, &bad);
    bad = p; bad.camera_position[2] = nan; check_bad_prepare(ctx, &bad);
    bad = p; bad.camera_up[0] = -inf; check_bad_prepare(ctx, &bad);
    bad = p; bad.camera_right[1] = nan; check_bad_prepare(ctx, &bad);
    bad = p; bad.camera_forward[2] = inf; check_bad_prepare(ctx, &bad);
    bad = p; bad.min_luminance = -0.5f; check_bad_prepare(ctx, &bad);
    bad = p; bad.max_luminance = nan; check_bad_prepare(ctx, &bad);
    bad = p; bad.min_luminance = 2.0f; check_bad_prepare(ctx, &bad);
    bad = p; bad.transfer = (uscale_transfer_function)3; check_bad_prepare(ctx, &bad);
    bad = p; bad.frame_id = 0; check_bad_prepare(ctx, &bad);
    bad = p; bad.reset = 2; check_bad_prepare(ctx, &bad);
    bad = p; bad.render_extent.width = 65; check_bad_prepare(ctx, &bad);
    bad = p; bad.depth.format = VK_FORMAT_D32_SFLOAT; check_bad_prepare(ctx, &bad);
    bad = p; bad.present_color.format = VK_FORMAT_R16G16B16A16_SFLOAT; check_bad_prepare(ctx, &bad);
    bad = p; bad.struct_size = 8;
    CHECK_INT(uscale_record_framegen_prepare(ctx, &bad), USCALE_ERROR_VERSION_MISMATCH);

    /* Frame 1, reset: two slots, slot 0 real but still presented. */
    CHECK_INT(uscale_record_framegen_prepare(ctx, &p), USCALE_OK);
    CHECK_INT(g_up_prepares, 1);
    CHECK_INT(g_up_prepare.frameId, 1);
    CHECK(g_up_prepare.motionVectorScaleX == 60.0f && g_up_prepare.motionVectorScaleY == 50.0f);
    CHECK(g_up_prepare.jitterOffsetX == 0.25f && g_up_prepare.jitterOffsetY == -0.25f);
    CHECK_INT(g_up_prepare.depth.usage, VK_IMAGE_USAGE_SAMPLED_BIT);
    CHECK_INT(g_up_prepare.reset, VK_TRUE);
    /* A second prepare before the dispatch. */
    bad = make_prepare(2, 0); check_bad_prepare(ctx, &bad);
    /* Stale or mismatched dispatches. */
    badd = make_dispatch(2, 1); check_bad_dispatch(ctx, &badd);
    badd = d; badd.present_color.image = FAKE(VkImage, 0x999); check_bad_dispatch(ctx, &badd);
    badd = d; badd.generated = badd.present_color; check_bad_dispatch(ctx, &badd);
    badd = d; badd.generated.image = p.depth.image; check_bad_dispatch(ctx, &badd);
    badd = d; badd.frame_time_ms = nan; check_bad_dispatch(ctx, &badd);
    /* Duplicate indices: an error, so the game recreates the swapchain. */
    badd = d; badd.acquired.real_index = badd.acquired.generated_index;
    check_bad_dispatch(ctx, &badd);
    /* A second image acquired without a pair would be stranded. */
    badd = d; badd.acquired.paired = 0; check_bad_dispatch(ctx, &badd);

    CHECK_INT(uscale_record_framegen_dispatch(ctx, &d, &plan), USCALE_OK);
    CHECK_INT(g_up_dispatches, 1);
    CHECK_INT(g_up_dispatch.reset, VK_TRUE);
    CHECK_INT(g_up_dispatch.output.usage, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    CHECK(g_up_dispatch.cameraFar == 1000.0f && g_up_dispatch.maxLuminance == 1.0f);
    CHECK_INT(g_up_dispatch.displayWidth, 128);
    CHECK(g_up_dispatch.distortionField.image == VK_NULL_HANDLE);
    CHECK_INT(plan.slot_count, 2);
    CHECK_INT(plan.slots[0].image_index, 2);
    CHECK_INT(plan.slots[0].use_generated, 0);
    CHECK_INT(plan.slots[1].image_index, 0);
    CHECK_INT(plan.slots[1].use_generated, 0);
    check_same_plan(&plan, &d.acquired, true, true);
    /* The prepare is used up. */
    check_bad_dispatch(ctx, &d);

    /* Frame 2: generated then real, exactly upstream's order. */
    p = make_prepare(2, 0);
    d = make_dispatch(2, 0);
    d.acquired.generated_index = 1;
    d.acquired.real_index = 3;
    CHECK_INT(uscale_record_framegen_prepare(ctx, &p), USCALE_OK);
    CHECK_INT(uscale_record_framegen_dispatch(ctx, &d, &plan), USCALE_OK);
    CHECK_INT(plan.slot_count, 2);
    CHECK_INT(plan.slots[0].image_index, 1);
    CHECK(plan.slots[0].image_available == d.acquired.generated_available);
    CHECK_INT(plan.slots[0].use_generated, 1);
    CHECK_INT(plan.slots[1].image_index, 3);
    CHECK(plan.slots[1].image_available == d.acquired.real_available);
    CHECK_INT(plan.slots[1].use_generated, 0);
    check_same_plan(&plan, &d.acquired, true, false);

    /* Frame 3: only the first image was acquired: one slot, present normally. */
    p = make_prepare(3, 0);
    d = make_dispatch(3, 0);
    d.acquired.real_acquired = 0;
    d.acquired.paired = 0;
    d.acquired.real_index = d.acquired.generated_index; /* ignored when unpaired */
    CHECK_INT(uscale_record_framegen_prepare(ctx, &p), USCALE_OK);
    CHECK_INT(uscale_record_framegen_dispatch(ctx, &d, &plan), USCALE_OK);
    CHECK_INT(plan.slot_count, 1);
    CHECK_INT(plan.slots[0].image_index, 2);
    CHECK_INT(plan.slots[0].use_generated, 0);
    check_same_plan(&plan, &d.acquired, true, false);

    /* Frame ids only go up. */
    bad = make_prepare(3, 0); check_bad_prepare(ctx, &bad);

    /* Frame 4: upstream rejects the interpolation after recording started:
     * the error is returned, the plan still lists both images, slot 0 real. */
    p = make_prepare(4, 0);
    d = make_dispatch(4, 0);
    g_up_dispatch_result = FFX_VK_FSR3_3_1_6_FRAMEGEN_ERROR_BACKEND;
    CHECK_INT(uscale_record_framegen_prepare(ctx, &p), USCALE_OK);
    CHECK_INT(uscale_record_framegen_dispatch(ctx, &d, &plan), USCALE_ERROR_VULKAN);
    CHECK_INT(plan.slot_count, 2);
    CHECK_INT(plan.slots[0].use_generated, 0);
    check_same_plan(&plan, &d.acquired, false, false);
    g_up_dispatch_result = FFX_VK_FSR3_3_1_6_FRAMEGEN_OK;
    /* That frame must be retired before the next prepare. */
    bad = make_prepare(5, 0); check_bad_prepare(ctx, &bad);
    CHECK_INT(uscale_retire_frame(ctx, 4), USCALE_OK);
    CHECK_INT(g_up_retired, 4);
    p = make_prepare(5, 0);
    CHECK_INT(uscale_record_framegen_prepare(ctx, &p), USCALE_OK);

    uscale_context_destroy(ctx);
    CHECK_INT(g_up_live, 0);
    CHECK_INT(g_live, 0);

    /* R16G16B16A16_SFLOAT is the other accepted format. */
    info.framegen_color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(g_up_create.colorFormat, VK_FORMAT_R16G16B16A16_SFLOAT);
    uscale_context_destroy(ctx);
    CHECK_INT(g_up_live, 0);
    uscale_framegen_fsr3_set_upstream(NULL);
}
#endif

int main(void)
{
    uscale_context_set_hooks(&k_hooks);
    test_old_struct_size();
    test_create_format();
#ifdef USCALE_HAVE_FFX
    test_ffx();
#else
    test_no_ffx();
#endif
    uscale_context_set_hooks(NULL);
    USCALE_TEST_END;
}
