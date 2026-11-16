/* Frame generation: upstream FSR 3.1.6 (ffx-vulkan::fsr3-vk-framegeneration-3.1.6)
 * plus its presenter policy (ffx-vulkan::framegeneration-presenter-policy).
 * Two steps per real frame, prepare then dispatch; the dispatch returns the
 * present plan built by ffxVkFrameGenerationBuildPresentPlan. The game owns
 * the swapchain. Present (code map: universal/src, roadmap 3.05).
 * SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "backend.h"

#ifdef USCALE_HAVE_FFX
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ffx_vk_framegeneration_presenter.h"

typedef struct fg_state {
    const uscale_fsr3fg_upstream *up;
    FfxVkFsr3_3_1_6FrameGenerationContext *context;
    VkFormat color_format;
    uscale_extent max_render;
    uscale_extent display;
    /* Upstream holds a prepare not yet consumed by a successful dispatch. */
    int pending;
    /* That prepare succeeded, so a dispatch for pending_id may follow. */
    int prepared;
    uint64_t pending_id;
    uint64_t last_id; /* last prepare's frame id; 0 before the first */
    /* What the dispatch reuses from its prepare. */
    VkImage color, depth, motion;
    uint32_t prepare_reset;
    float camera_near, camera_far, camera_fov_y, view_space_to_meters;
    float min_luminance, max_luminance;
    uscale_transfer_function transfer;
} fg_state;

static const uscale_fsr3fg_upstream k_upstream = {
    ffxVkFsr3_3_1_6FrameGenerationContextCreate,
    ffxVkFsr3_3_1_6FrameGenerationContextRecordPrepare,
    ffxVkFsr3_3_1_6FrameGenerationContextRecordDispatch,
    ffxVkFsr3_3_1_6FrameGenerationContextRetireFrame,
    ffxVkFsr3_3_1_6FrameGenerationContextDestroy
};
static const uscale_fsr3fg_upstream *g_upstream = NULL;

void uscale_framegen_fsr3_set_upstream(const uscale_fsr3fg_upstream *upstream)
{
    g_upstream = upstream;
}

static uscale_result fg_from_upstream(FfxVkFsr3_3_1_6FrameGenerationResult r)
{
    switch (r) {
    case FFX_VK_FSR3_3_1_6_FRAMEGEN_OK:
        return USCALE_OK;
    case FFX_VK_FSR3_3_1_6_FRAMEGEN_ERROR_INVALID_ARGUMENT:
    case FFX_VK_FSR3_3_1_6_FRAMEGEN_ERROR_IN_FLIGHT:
        return USCALE_ERROR_INVALID_ARGUMENT;
    case FFX_VK_FSR3_3_1_6_FRAMEGEN_ERROR_OUT_OF_MEMORY:
        return USCALE_ERROR_OUT_OF_MEMORY;
    default:
        return USCALE_ERROR_VULKAN;
    }
}

/* Upstream samples the real frame and copies it into its history. */
#define FG_COLOR_USAGE (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)

/* The rules of upstream's bridge header, checked before anything is recorded. */
static int fg_finite(float v)
{
    return isfinite(v) ? 1 : 0;
}

static int positive(float v)
{
    return fg_finite(v) && v > 0.0f;
}

static int finite3(const float v[3])
{
    return fg_finite(v[0]) && fg_finite(v[1]) && fg_finite(v[2]);
}

static int flag(uint32_t v)
{
    return v == 0u || v == 1u;
}

static FfxVkFsr3_3_1_6FrameGenerationImage fg_image(const uscale_image *image,
                                                    VkImageUsageFlags usage)
{
    FfxVkFsr3_3_1_6FrameGenerationImage out;

    memset(&out, 0, sizeof(out));
    out.image = image->image;
    out.format = image->format;
    out.width = image->extent.width;
    out.height = image->extent.height;
    out.layout = image->layout;
    out.usage = usage;
    return out;
}

uscale_result uscale_framegen_create(const uscale_context_create_info *info, void **out_state)
{
    FfxVkFsr3_3_1_6FrameGenerationCreateInfo create;
    fg_state *st;
    uscale_result r;

    if (out_state == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    *out_state = NULL;
    if (info == NULL || info->device == VK_NULL_HANDLE ||
        info->physical_device == VK_NULL_HANDLE ||
        info->max_render_extent.width == 0 || info->max_render_extent.height == 0 ||
        info->display_extent.width == 0 || info->display_extent.height == 0)
        return USCALE_ERROR_INVALID_ARGUMENT;
    if (info->framegen_color_format != VK_FORMAT_R8G8B8A8_UNORM &&
        info->framegen_color_format != VK_FORMAT_R16G16B16A16_SFLOAT)
        return USCALE_ERROR_UNSUPPORTED;

    st = (fg_state *)calloc(1, sizeof(*st));
    if (st == NULL)
        return USCALE_ERROR_OUT_OF_MEMORY;
    st->up = g_upstream != NULL ? g_upstream : &k_upstream;
    memset(&create, 0, sizeof(create));
    create.physicalDevice = info->physical_device;
    create.device = info->device;
    create.maxRenderWidth = info->max_render_extent.width;
    create.maxRenderHeight = info->max_render_extent.height;
    create.displayWidth = info->display_extent.width;
    create.displayHeight = info->display_extent.height;
    create.colorFormat = info->framegen_color_format;
    r = fg_from_upstream(st->up->create(&create, &st->context));
    if (r != USCALE_OK) {
        free(st);
        return r;
    }
    st->color_format = info->framegen_color_format;
    st->max_render = info->max_render_extent;
    st->display = info->display_extent;
    *out_state = st;
    return USCALE_OK;
}

static int prepare_valid(const fg_state *st, const uscale_framegen_prepare_info *info)
{
    return info->command_buffer != VK_NULL_HANDLE &&
           fg_finite(info->jitter_x) && fg_finite(info->jitter_y) &&
           positive(info->frame_time_ms) &&
           positive(info->camera_near) && positive(info->camera_far) &&
           info->camera_far > info->camera_near &&
           positive(info->camera_fov_y) && positive(info->view_space_to_meters) &&
           finite3(info->camera_position) && finite3(info->camera_up) &&
           finite3(info->camera_right) && finite3(info->camera_forward) &&
           fg_finite(info->min_luminance) && info->min_luminance >= 0.0f &&
           fg_finite(info->max_luminance) && info->max_luminance >= info->min_luminance &&
           (info->transfer == USCALE_TRANSFER_SRGB || info->transfer == USCALE_TRANSFER_PQ ||
            info->transfer == USCALE_TRANSFER_SCRGB) &&
           flag(info->reset) &&
           info->frame_id != 0u && info->frame_id > st->last_id &&
           info->render_extent.width != 0 && info->render_extent.height != 0 &&
           info->render_extent.width <= st->max_render.width &&
           info->render_extent.height <= st->max_render.height &&
           info->present_color.image != VK_NULL_HANDLE &&
           info->present_color.format == st->color_format &&
           info->depth.image != VK_NULL_HANDLE && info->depth.format == VK_FORMAT_R32_SFLOAT &&
           info->motion.image != VK_NULL_HANDLE &&
           info->motion.format == VK_FORMAT_R16G16_SFLOAT;
}

uscale_result uscale_framegen_prepare(void *state, const uscale_framegen_prepare_info *info)
{
    fg_state *st = (fg_state *)state;
    FfxVkFsr3_3_1_6FrameGenerationPrepareInfo p;
    FfxVkFsr3_3_1_6FrameGenerationResult ur;

    if (st == NULL || info == NULL || !prepare_valid(st, info))
        return USCALE_ERROR_INVALID_ARGUMENT;
    if (st->pending)
        return USCALE_ERROR_INVALID_ARGUMENT; /* dispatch it, or retire it, first */

    memset(&p, 0, sizeof(p));
    p.commandBuffer = info->command_buffer;
    p.color = fg_image(&info->present_color, FG_COLOR_USAGE);
    p.depth = fg_image(&info->depth, VK_IMAGE_USAGE_SAMPLED_BIT);
    p.motionVectors = fg_image(&info->motion, VK_IMAGE_USAGE_SAMPLED_BIT);
    p.renderWidth = info->render_extent.width;
    p.renderHeight = info->render_extent.height;
    /* Same conventions as backend_fsr3.c: render-pixel jitter unchanged, UV
     * motion scaled to pixels by the active render extent. */
    p.jitterOffsetX = info->jitter_x;
    p.jitterOffsetY = info->jitter_y;
    p.motionVectorScaleX = (float)info->render_extent.width;
    p.motionVectorScaleY = (float)info->render_extent.height;
    p.frameTimeMilliseconds = info->frame_time_ms;
    p.minLuminance = info->min_luminance;
    p.maxLuminance = info->max_luminance;
    p.transferFunction = (FfxVkFsr3_3_1_6FrameGenerationTransferFunction)info->transfer;
    p.cameraNear = info->camera_near;
    p.cameraFar = info->camera_far;
    p.viewSpaceToMeters = info->view_space_to_meters;
    p.cameraVerticalFovRadians = info->camera_fov_y;
    memcpy(p.cameraPosition, info->camera_position, sizeof(p.cameraPosition));
    memcpy(p.cameraUp, info->camera_up, sizeof(p.cameraUp));
    memcpy(p.cameraRight, info->camera_right, sizeof(p.cameraRight));
    memcpy(p.cameraForward, info->camera_forward, sizeof(p.cameraForward));
    p.frameId = info->frame_id;
    p.reset = info->reset != 0 ? VK_TRUE : VK_FALSE;

    ur = st->up->prepare(st->context, &p);
    if (ur == FFX_VK_FSR3_3_1_6_FRAMEGEN_ERROR_INVALID_ARGUMENT)
        return USCALE_ERROR_INVALID_ARGUMENT; /* upstream recorded nothing */
    /* Past upstream's checks the frame is held until dispatched or retired. */
    st->pending = 1;
    st->pending_id = info->frame_id;
    st->last_id = info->frame_id;
    if (ur != FFX_VK_FSR3_3_1_6_FRAMEGEN_OK)
        return fg_from_upstream(ur);
    st->prepared = 1;
    st->color = info->present_color.image;
    st->depth = info->depth.image;
    st->motion = info->motion.image;
    st->prepare_reset = info->reset;
    st->camera_near = info->camera_near;
    st->camera_far = info->camera_far;
    st->camera_fov_y = info->camera_fov_y;
    st->view_space_to_meters = info->view_space_to_meters;
    st->min_luminance = info->min_luminance;
    st->max_luminance = info->max_luminance;
    st->transfer = info->transfer;
    return USCALE_OK;
}

/* Copies upstream's plan slot by slot, in its order (generated then real). */
static int build_plan(const uscale_acquired_images *a, bool dispatched, bool reset,
                      uscale_present_plan *out_plan)
{
    FfxVkFrameGenerationAcquiredPair pair;
    FfxVkFrameGenerationPresentPlan plan;
    uint32_t i;

    memset(&pair, 0, sizeof(pair));
    pair.generatedImageIndex = a->generated_index;
    pair.realImageIndex = a->real_index;
    pair.generatedAvailableSemaphore = a->generated_available;
    pair.realAvailableSemaphore = a->real_available;
    pair.generatedAcquireResult = a->generated_acquired ? VK_SUCCESS : VK_NOT_READY;
    pair.realAcquireResult = a->real_acquired ? VK_SUCCESS : VK_NOT_READY;
    pair.generatedImageAcquired = a->generated_acquired != 0;
    pair.realImageAcquired = a->real_acquired != 0;
    pair.paired = a->paired != 0;
    if (!ffxVkFrameGenerationBuildPresentPlan(&pair, dispatched, reset, &plan) ||
        plan.slotCount == 0u || plan.slotCount > 2u)
        return 0;
    memset(out_plan, 0, sizeof(*out_plan));
    for (i = 0; i < plan.slotCount; i++) {
        out_plan->slots[i].image_index = plan.slots[i].imageIndex;
        out_plan->slots[i].image_available = plan.slots[i].imageAvailableSemaphore;
        out_plan->slots[i].use_generated = plan.slots[i].useInterpolatedScene ? 1u : 0u;
    }
    out_plan->slot_count = plan.slotCount;
    return 1;
}

uscale_result uscale_framegen_dispatch(void *state, const uscale_framegen_dispatch_info *info,
                                       uscale_present_plan *out_plan)
{
    fg_state *st = (fg_state *)state;
    const uscale_acquired_images *a;
    FfxVkFsr3_3_1_6FrameGenerationDispatchInfo d;
    uscale_present_plan check;
    VkImage out;
    bool reset;
    uscale_result r;

    if (st == NULL || info == NULL || out_plan == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    memset(out_plan, 0, sizeof(*out_plan));
    a = &info->acquired;
    out = info->generated.image;
    /* A dispatch follows its own successful prepare. */
    if (!st->prepared || info->frame_id != st->pending_id)
        return USCALE_ERROR_INVALID_ARGUMENT;
    if (info->command_buffer == VK_NULL_HANDLE || !positive(info->frame_time_ms) ||
        !flag(info->reset) || !flag(a->generated_acquired) || !flag(a->real_acquired) ||
        !flag(a->paired) ||
        info->present_color.image != st->color ||
        info->present_color.format != st->color_format ||
        out == VK_NULL_HANDLE || info->generated.format != st->color_format ||
        out == st->color || out == st->depth || out == st->motion ||
        (info->distortion.image != VK_NULL_HANDLE &&
         (info->distortion.format != VK_FORMAT_R16G16_SFLOAT || out == info->distortion.image)))
        return USCALE_ERROR_INVALID_ARGUMENT;
    reset = info->reset != 0 || st->prepare_reset != 0;
    /* An outcome upstream cannot plan (duplicate indices, a stranded second
     * image) is refused before anything is recorded. */
    if (!build_plan(a, true, reset, &check))
        return USCALE_ERROR_INVALID_ARGUMENT;

    memset(&d, 0, sizeof(d));
    d.commandBuffer = info->command_buffer;
    d.color = fg_image(&info->present_color, FG_COLOR_USAGE);
    d.output = fg_image(&info->generated, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    if (info->distortion.image != VK_NULL_HANDLE)
        d.distortionField = fg_image(&info->distortion, VK_IMAGE_USAGE_SAMPLED_BIT);
    d.displayWidth = st->display.width;
    d.displayHeight = st->display.height;
    /* All-zero interpolation rectangle: the whole display. */
    d.frameTimeMilliseconds = info->frame_time_ms;
    d.cameraNear = st->camera_near;
    d.cameraFar = st->camera_far;
    d.viewSpaceToMeters = st->view_space_to_meters;
    d.cameraVerticalFovRadians = st->camera_fov_y;
    d.minLuminance = st->min_luminance;
    d.maxLuminance = st->max_luminance;
    d.transferFunction = (FfxVkFsr3_3_1_6FrameGenerationTransferFunction)st->transfer;
    d.frameId = info->frame_id;
    d.reset = reset ? VK_TRUE : VK_FALSE;

    r = fg_from_upstream(st->up->dispatch(st->context, &d));
    if (r == USCALE_OK) {
        st->pending = 0;
        st->prepared = 0;
    } else if (r == USCALE_ERROR_INVALID_ARGUMENT) {
        return r; /* upstream checked and recorded nothing; the prepare stands */
    } else {
        st->prepared = 0; /* partial work: retire this frame before the next prepare */
    }
    (void)build_plan(a, r == USCALE_OK, reset, out_plan);
    return r;
}

uscale_result uscale_framegen_retire(void *state, uint64_t frame_id)
{
    fg_state *st = (fg_state *)state;
    uscale_result r;

    if (st == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    r = fg_from_upstream(st->up->retire(st->context, frame_id));
    if (r == USCALE_OK && st->pending && st->pending_id <= frame_id) {
        st->pending = 0; /* upstream drops a held prepare the same way */
        st->prepared = 0;
    }
    return r;
}

void uscale_framegen_destroy(void *state)
{
    fg_state *st = (fg_state *)state;

    if (st == NULL)
        return;
    st->up->destroy(st->context);
    free(st);
}
#else
/* Built without USCALE_WITH_FFX: the context may ask for frame generation,
 * but both record calls report it unsupported. */
uscale_result uscale_framegen_create(const uscale_context_create_info *info, void **out_state)
{
    (void)info;
    if (out_state == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    *out_state = NULL;
    return USCALE_OK;
}

uscale_result uscale_framegen_prepare(void *state, const uscale_framegen_prepare_info *info)
{
    (void)state;
    (void)info;
    return USCALE_ERROR_UNSUPPORTED;
}

uscale_result uscale_framegen_dispatch(void *state, const uscale_framegen_dispatch_info *info,
                                       uscale_present_plan *out_plan)
{
    (void)state;
    (void)info;
    (void)out_plan;
    return USCALE_ERROR_UNSUPPORTED;
}

uscale_result uscale_framegen_retire(void *state, uint64_t frame_id)
{
    (void)state;
    (void)frame_id;
    return USCALE_OK;
}

void uscale_framegen_destroy(void *state)
{
    (void)state;
}
#endif
