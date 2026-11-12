/* Backend FSR4: upstream FSR 4 v07 provider (ffx-vulkan::fsr4-v07-vulkan) with the game's
 * assets, read only from fsr4_asset_dir. We ship and commit no model file.
 * Create is filled; record_upscale stays a stub. SPDX-License-Identifier: MIT */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS /* MSVC /W4 /WX: fopen and getenv are fine here */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backend.h"
#include "backend_fsr4.h"

#ifdef USCALE_HAVE_FFX
#include "ffx_vk_fsr4_v07.h"
#include "ffx_vk_fsr4_v07_assets.h"

_Static_assert(USCALE_FSR4_PASS_COUNT == FFX_FSR4_VK_PASS_COUNT, "pass count");
_Static_assert(FFX_FSR4_MODEL_PASS_COUNT + 4u == USCALE_FSR4_PASS_COUNT, "pass layout");
#define USCALE_FSR4_INITIALIZER_BYTES FFX_FSR4_V07_INITIALIZER_BYTES
#define USCALE_FSR4_PRE_WEIGHTS_BYTES FFX_FSR4_V07_PRE_PASS_WEIGHTS_BYTES
#else
#define USCALE_FSR4_INITIALIZER_BYTES 89216u
#define USCALE_FSR4_PRE_WEIGHTS_BYTES 1024u
#endif

uscale_result uscale_fsr4_check_caps(const uscale_device_caps *caps, const char **out_reason)
{
    const char *reason = NULL;

    if (caps == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    if (!caps->has_int_dot_product)
        reason = "FSR 4 unavailable: no integer dot product";
    else if (!caps->has_fp16)
        reason = "FSR 4 unavailable: no FP16";
    if (out_reason != NULL)
        *out_reason = reason;
    return reason == NULL ? USCALE_OK : USCALE_ERROR_UNSUPPORTED;
}

#ifdef USCALE_HAVE_FFX
static uscale_result join_path(char *out, const char *dir, const char *name)
{
    int n = snprintf(out, USCALE_FSR4_PATH_MAX, "%s/%s", dir, name);
    return (n > 0 && (size_t)n < USCALE_FSR4_PATH_MAX) ? USCALE_OK : USCALE_ERROR_INVALID_ARGUMENT;
}

uscale_result uscale_fsr4_build_paths(const char *asset_dir, uscale_quality quality,
                                      uscale_extent display, uscale_fsr4_paths *out)
{
    FfxFsr4V07AssetSet set;
    uscale_fsr4_paths paths;
    uscale_result r;
    unsigned i, k = 0;

    if (out == NULL || (unsigned)quality >= (unsigned)USCALE_QUALITY_COUNT)
        return USCALE_ERROR_INVALID_ARGUMENT;
    if (asset_dir == NULL || asset_dir[0] == '\0')
        return USCALE_ERROR_RUNTIME_MISSING;
    /* uscale_quality values 0..4 are FfxFsr4ModelPreset 0..4 (native AA .. ultra performance). */
    if (!ffxFsr4V07BuildAssetSet((FfxFsr4ModelPreset)quality, display.width, display.height, &set))
        return USCALE_ERROR_UNSUPPORTED; /* zero or too large a display size */

    memset(&paths, 0, sizeof(paths));
    r = join_path(paths.shader[k++], asset_dir, set.pre);
    for (i = 0; r == USCALE_OK && i < FFX_FSR4_MODEL_PASS_COUNT; i++)
        r = join_path(paths.shader[k++], asset_dir, set.model[i]);
    if (r == USCALE_OK)
        r = join_path(paths.shader[k++], asset_dir, set.post);
    if (r == USCALE_OK)
        r = join_path(paths.shader[k++], asset_dir, set.rcas);
    if (r == USCALE_OK)
        r = join_path(paths.shader[k++], asset_dir, set.spdAutoExposure);
    if (r == USCALE_OK)
        r = join_path(paths.initializer, asset_dir, set.initializer);
    if (r == USCALE_OK)
        r = join_path(paths.pre_weights, asset_dir, set.prePassWeights);
    if (r != USCALE_OK)
        return r;
    *out = paths;
    return USCALE_OK;
}
#else
uscale_result uscale_fsr4_build_paths(const char *asset_dir, uscale_quality quality,
                                      uscale_extent display, uscale_fsr4_paths *out)
{
    (void)asset_dir;
    (void)quality;
    (void)display;
    (void)out;
    return USCALE_ERROR_UNSUPPORTED; /* built without upstream's FSR 4 targets */
}
#endif

/* Whole file into memory. Missing or unreadable: RUNTIME_MISSING. */
static uscale_result read_file(const char *path, void **out, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    long len;
    void *buf;

    if (f == NULL)
        return USCALE_ERROR_RUNTIME_MISSING;
    if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return USCALE_ERROR_RUNTIME_MISSING;
    }
    buf = malloc(len > 0 ? (size_t)len : 1u);
    if (buf == NULL) {
        fclose(f);
        return USCALE_ERROR_OUT_OF_MEMORY;
    }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        fclose(f);
        return USCALE_ERROR_RUNTIME_MISSING;
    }
    fclose(f);
    *out = buf;
    *out_size = (size_t)len;
    return USCALE_OK;
}

void uscale_fsr4_assets_free(uscale_fsr4_assets *a)
{
    unsigned i;

    if (a == NULL)
        return;
    for (i = 0; i < USCALE_FSR4_PASS_COUNT; i++)
        free(a->shader[i]);
    free(a->initializer);
    free(a->pre_weights);
    memset(a, 0, sizeof(*a));
}

uscale_result uscale_fsr4_load_assets(const uscale_fsr4_paths *paths, uscale_fsr4_assets *out)
{
    uscale_fsr4_assets a;
    uscale_result r = USCALE_OK;
    unsigned i;

    if (paths == NULL || out == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    memset(&a, 0, sizeof(a));
    for (i = 0; r == USCALE_OK && i < USCALE_FSR4_PASS_COUNT; i++) {
        r = read_file(paths->shader[i], &a.shader[i], &a.shader_size[i]);
        if (r == USCALE_OK && (a.shader_size[i] == 0 || a.shader_size[i] % 4u != 0))
            r = USCALE_ERROR_INVALID_ARGUMENT; /* SPIR-V is whole 32-bit words */
    }
    if (r == USCALE_OK) {
        r = read_file(paths->initializer, &a.initializer, &a.initializer_size);
        if (r == USCALE_OK && a.initializer_size != USCALE_FSR4_INITIALIZER_BYTES)
            r = USCALE_ERROR_INVALID_ARGUMENT;
    }
    if (r == USCALE_OK) {
        r = read_file(paths->pre_weights, &a.pre_weights, &a.pre_weights_size);
        if (r == USCALE_OK && a.pre_weights_size != USCALE_FSR4_PRE_WEIGHTS_BYTES)
            r = USCALE_ERROR_INVALID_ARGUMENT;
    }
    if (r != USCALE_OK) {
        uscale_fsr4_assets_free(&a);
        return r;
    }
    *out = a;
    return USCALE_OK;
}

#ifdef USCALE_HAVE_FFX

typedef struct fsr4_state {
    FfxInterface iface;     /* filled by ffxFsr4VkCreateContext; iface.device is the VK context */
    ffxContext ctx;         /* upstream's v07 provider context */
    void *scratch;          /* backend bookkeeping; must outlive iface */
    uint32_t profile_flags; /* BB_FSR4_PROFILE / BB_FSR4_STATS as seen at create */
} fsr4_state;

static uscale_result fsr4_create(const uscale_context_create_info *info, void **out_state)
{
    uscale_device_caps caps;
    uscale_fsr4_paths *paths = NULL;
    uscale_fsr4_assets assets;
    FfxFsr4VkCreateInfo ci;
    ffxCreateContextDescUpscale desc;
    fsr4_state *s = NULL;
    uscale_result r;
    unsigned i;

    if (out_state != NULL)
        *out_state = NULL;
    if (info == NULL || out_state == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    if (info->device == VK_NULL_HANDLE || info->physical_device == VK_NULL_HANDLE)
        return USCALE_ERROR_INVALID_ARGUMENT;

    /* Capability gate first; the folder test below is the same one uscale_query_caps does. */
    r = uscale_query_caps(info->physical_device, info->get_instance_proc_addr, info->instance,
                          info->fsr4_asset_dir, &caps);
    if (r != USCALE_OK)
        return r;
    r = uscale_fsr4_check_caps(&caps, NULL);
    if (r != USCALE_OK)
        return r;

    paths = (uscale_fsr4_paths *)malloc(sizeof(*paths));
    s = (fsr4_state *)calloc(1, sizeof(*s));
    if (paths == NULL || s == NULL) {
        r = USCALE_ERROR_OUT_OF_MEMORY;
        goto done;
    }
    r = uscale_fsr4_build_paths(info->fsr4_asset_dir, info->quality, info->display_extent, paths);
    if (r != USCALE_OK)
        goto done;
    r = uscale_fsr4_load_assets(paths, &assets);
    if (r != USCALE_OK)
        goto done;

    s->scratch = calloc(1, ffxFsr4VkGetScratchMemorySize());
    if (s->scratch == NULL) {
        r = USCALE_ERROR_OUT_OF_MEMORY;
        goto assets;
    }
    memset(&ci, 0, sizeof(ci));
    ci.device = info->device;
    ci.physicalDevice = info->physical_device;
    for (i = 0; i < USCALE_FSR4_PASS_COUNT; i++) {
        ci.shaders[i].spirv = (const uint32_t *)assets.shader[i];
        ci.shaders[i].sizeBytes = assets.shader_size[i];
        ci.shaders[i].entryPoint = "main";
    }
    ci.modelInitializer = assets.initializer;
    ci.modelInitializerSize = assets.initializer_size;
    ci.prePassWeights = assets.pre_weights;
    ci.prePassWeightsSize = assets.pre_weights_size;
    ci.scratchBuffer = s->scratch;
    ci.scratchBufferSize = ffxFsr4VkGetScratchMemorySize();
    ci.modelVariant = FFX_FSR4_VARIANT_V07_INT8;

    /* Upstream reads BB_FSR4_PROFILE and BB_FSR4_STATS itself when it builds pipelines and
     * the timestamp pool, so the switches pass straight through; we record what was set. */
    s->profile_flags = uscale_profile_flags_from_env(getenv("BB_FSR4_PROFILE"),
                                                     getenv("BB_FSR4_STATS"));

    if (ffxFsr4VkCreateContext(&ci, &s->iface) != VK_SUCCESS) {
        r = USCALE_ERROR_VULKAN;
        goto assets;
    }
    memset(&desc, 0, sizeof(desc));
    desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
    desc.flags = FFX_UPSCALE_ENABLE_AUTO_EXPOSURE; /* the API carries no exposure input */
    if (info->hdr != 0)
        desc.flags |= FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE;
    desc.maxRenderSize.width = info->max_render_extent.width;
    desc.maxRenderSize.height = info->max_render_extent.height;
    desc.maxUpscaleSize.width = info->display_extent.width;
    desc.maxUpscaleSize.height = info->display_extent.height;
    ffxFsr4V07SetBackendInterface(&s->iface);
    if (ffxFsr4V07CreateContext(&s->ctx, &desc.header, NULL) != FFX_API_RETURN_OK) {
        ffxFsr4V07SetBackendInterface(NULL);
        ffxFsr4VkDestroyContext((FfxFsr4VkContext *)s->iface.device);
        r = USCALE_ERROR_VULKAN;
        goto assets;
    }
    ffxFsr4V07SetBackendInterface(NULL); /* the provider copied it */
    r = USCALE_OK;

assets:
    uscale_fsr4_assets_free(&assets); /* the backend deep-copied what it needs */
done:
    free(paths);
    if (r != USCALE_OK) {
        if (s != NULL)
            free(s->scratch);
        free(s);
        return r;
    }
    *out_state = s;
    return USCALE_OK;
}

static void fsr4_destroy(void *state)
{
    fsr4_state *s = (fsr4_state *)state;

    if (s == NULL)
        return;
    /* Provider context first, then the Vulkan backend it used (upstream's order). */
    ffxFsr4V07DestroyContext(&s->ctx, NULL);
    ffxFsr4VkDestroyContext((FfxFsr4VkContext *)s->iface.device);
    free(s->scratch);
    free(s);
}

#else /* built without upstream's FSR 4 targets */

static uscale_result fsr4_create(const uscale_context_create_info *info, void **out_state)
{
    (void)info;
    if (out_state != NULL)
        *out_state = NULL;
    return USCALE_ERROR_UNSUPPORTED;
}

static void fsr4_destroy(void *state)
{
    (void)state;
}

#endif

static uscale_result fsr4_record_upscale(void *state, const uscale_upscale_info *info)
{
    (void)state;
    (void)info;
    return USCALE_ERROR_NOT_IMPLEMENTED; /* the per-frame dispatch is not in 5.03 */
}

static const uscale_backend_ops fsr4_ops = {
    USCALE_BACKEND_FSR4, fsr4_create, fsr4_record_upscale, fsr4_destroy
};

const uscale_backend_ops *uscale_backend_fsr4_ops(void)
{
    return &fsr4_ops;
}
