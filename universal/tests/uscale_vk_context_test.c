/* Context lifecycle with fake backend ops: backend choice, FSR 3 fallback, routing and
 * destroy. Fake handles are never dereferenced; no GPU needed. SPDX-License-Identifier: MIT */
#include <stdlib.h>

#include "backend.h"
#include "uscale_test.h"

static uscale_device_caps g_caps;
static int g_live; /* backend and framegen states not yet destroyed */
static int g_create_calls[USCALE_BACKEND_COUNT];
static int g_fail_create[USCALE_BACKEND_COUNT];
static int g_upscale_calls[USCALE_BACKEND_COUNT];
static int g_destroy_calls[USCALE_BACKEND_COUNT];
static int g_fg_recorded, g_fg_destroyed, g_fg_fail;
static uint64_t g_fg_retired;

typedef struct fake_state {
    uscale_backend id;
} fake_state;

static uscale_result fake_create(uscale_backend id, void **out_state)
{
    fake_state *s;
    g_create_calls[id]++;
    if (g_fail_create[id])
        return USCALE_ERROR_RUNTIME_MISSING;
    s = (fake_state *)malloc(sizeof(*s));
    if (s == NULL)
        return USCALE_ERROR_OUT_OF_MEMORY;
    s->id = id;
    g_live++;
    *out_state = s;
    return USCALE_OK;
}

static uscale_result fsr3_create(const uscale_context_create_info *i, void **o)
{
    (void)i;
    return fake_create(USCALE_BACKEND_FSR3, o);
}
static uscale_result fsr4_create(const uscale_context_create_info *i, void **o)
{
    (void)i;
    return fake_create(USCALE_BACKEND_FSR4, o);
}
static uscale_result xess_create(const uscale_context_create_info *i, void **o)
{
    (void)i;
    return fake_create(USCALE_BACKEND_XESS, o);
}
static uscale_result dlss_create(const uscale_context_create_info *i, void **o)
{
    (void)i;
    return fake_create(USCALE_BACKEND_DLSS, o);
}

static uscale_result fake_upscale(void *state, const uscale_upscale_info *info)
{
    (void)info;
    g_upscale_calls[((fake_state *)state)->id]++;
    return USCALE_OK;
}

static void fake_destroy(void *state)
{
    g_destroy_calls[((fake_state *)state)->id]++;
    g_live--;
    free(state);
}

static const uscale_backend_ops k_ops[USCALE_BACKEND_COUNT] = {
    {USCALE_BACKEND_AUTO, NULL, NULL, NULL},
    {USCALE_BACKEND_FSR3, fsr3_create, fake_upscale, fake_destroy},
    {USCALE_BACKEND_FSR4, fsr4_create, fake_upscale, fake_destroy},
    {USCALE_BACKEND_XESS, xess_create, fake_upscale, fake_destroy},
    {USCALE_BACKEND_DLSS, dlss_create, fake_upscale, fake_destroy},
};

static const uscale_backend_ops *fake_ops_for(uscale_backend id)
{
    return (int)id > 0 && (int)id < (int)USCALE_BACKEND_COUNT ? &k_ops[id] : NULL;
}

static uscale_result fake_caps(const uscale_context_create_info *info, uscale_device_caps *out)
{
    (void)info;
    *out = g_caps;
    return USCALE_OK;
}

static uscale_result fg_create(const uscale_context_create_info *i, void **o)
{
    (void)i;
    if (g_fg_fail)
        return USCALE_ERROR_VULKAN;
    *o = malloc(1);
    g_live++;
    return USCALE_OK;
}
static uscale_result fg_record(void *s, const uscale_framegen_info *i)
{
    (void)s;
    (void)i;
    g_fg_recorded++;
    return USCALE_OK;
}
static uscale_result fg_retire(void *s, uint64_t id)
{
    (void)s;
    g_fg_retired = id;
    return USCALE_OK;
}
static void fg_destroy(void *s)
{
    g_fg_destroyed++;
    g_live--;
    free(s);
}

static const uscale_framegen_ops k_fg = {fg_create, fg_record, fg_retire, fg_destroy};
static const uscale_context_hooks k_hooks = {fake_ops_for, fake_caps, &k_fg};

static void reset(void)
{
    uscale_device_caps zero = {0};
    int i;
    g_caps = zero;
    g_caps.struct_size = sizeof(g_caps);
    g_live = 0;
    for (i = 0; i < (int)USCALE_BACKEND_COUNT; i++) {
        g_create_calls[i] = 0;
        g_fail_create[i] = 0;
        g_upscale_calls[i] = 0;
        g_destroy_calls[i] = 0;
    }
    g_fg_recorded = g_fg_destroyed = g_fg_fail = 0;
    g_fg_retired = 0;
}

static uscale_context_create_info make_info(uscale_backend preferred)
{
    uscale_context_create_info info = {0};
    info.struct_size = sizeof(info);
    info.api_version = USCALE_VERSION;
    info.preferred_backend = preferred;
    return info;
}

int main(void)
{
    uscale_context_create_info info;
    uscale_context *ctx = NULL;
    const char *reason = NULL;
    uscale_upscale_info up = {0};
    uscale_framegen_info fg = {0};

    uscale_context_set_hooks(&k_hooks);
    up.struct_size = sizeof(up);
    fg.struct_size = sizeof(fg);

    /* AUTO on a plain device: FSR 3, created once. */
    reset();
    info = make_info(USCALE_BACKEND_AUTO);
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(uscale_context_backend(ctx, &reason), USCALE_BACKEND_FSR3);
    CHECK_STR(reason, "auto: FSR 3 is the best available");
    CHECK_INT(g_create_calls[USCALE_BACKEND_FSR3], 1);
    uscale_context_destroy(ctx);
    CHECK_INT(g_live, 0);
    CHECK_INT(g_destroy_calls[USCALE_BACKEND_FSR3], 1);

    /* AUTO with XeSS available: XeSS is chosen and record calls route to it. */
    reset();
    g_caps.xess_runtime_available = 1;
    g_caps.has_int_dot_product = 1;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(uscale_context_backend(ctx, &reason), USCALE_BACKEND_XESS);
    CHECK_STR(reason, "auto: XeSS is the best available");
    CHECK_INT(uscale_record_upscale(ctx, &up), USCALE_OK);
    CHECK_INT(uscale_record_upscale(ctx, &up), USCALE_OK);
    CHECK_INT(g_upscale_calls[USCALE_BACKEND_XESS], 2);
    CHECK_INT(g_upscale_calls[USCALE_BACKEND_FSR3], 0);
    CHECK_INT(g_create_calls[USCALE_BACKEND_FSR3], 0);
    uscale_context_destroy(ctx);
    CHECK_INT(g_live, 0);

    /* The chosen backend's create fails: FSR 3 runs instead, and the reason says so. */
    reset();
    g_caps.xess_runtime_available = 1;
    g_caps.has_int_dot_product = 1;
    g_fail_create[USCALE_BACKEND_XESS] = 1;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(uscale_context_backend(ctx, &reason), USCALE_BACKEND_FSR3);
    CHECK_STR(reason, "fallback: XeSS failed to start, using FSR 3");
    CHECK_INT(g_create_calls[USCALE_BACKEND_XESS], 1);
    CHECK_INT(g_create_calls[USCALE_BACKEND_FSR3], 1);
    CHECK_INT(uscale_record_upscale(ctx, &up), USCALE_OK);
    CHECK_INT(g_upscale_calls[USCALE_BACKEND_FSR3], 1);
    CHECK_INT(g_upscale_calls[USCALE_BACKEND_XESS], 0);
    uscale_context_destroy(ctx);
    CHECK_INT(g_live, 0);
    CHECK_INT(g_destroy_calls[USCALE_BACKEND_XESS], 0);
    CHECK_INT(g_destroy_calls[USCALE_BACKEND_FSR3], 1);

    /* A named backend the device cannot run: selection falls back, its create is never tried. */
    reset();
    info = make_info(USCALE_BACKEND_DLSS);
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(uscale_context_backend(ctx, &reason), USCALE_BACKEND_FSR3);
    CHECK_STR(reason, "DLSS unavailable: not an NVIDIA GPU");
    CHECK_INT(g_create_calls[USCALE_BACKEND_DLSS], 0);
    uscale_context_destroy(ctx);
    CHECK_INT(g_live, 0);

    /* FSR 3 itself fails: create fails, nothing leaks, *out_context is NULL. */
    reset();
    g_fail_create[USCALE_BACKEND_FSR3] = 1;
    info = make_info(USCALE_BACKEND_AUTO);
    ctx = (uscale_context *)&info;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_ERROR_RUNTIME_MISSING);
    CHECK(ctx == NULL);
    CHECK_INT(g_live, 0);

    /* Frame generation: created, routed, retired and destroyed with the context. */
    reset();
    info.enable_frame_generation = 1;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(g_live, 2);
    CHECK_INT(uscale_record_framegen(ctx, &fg), USCALE_OK);
    CHECK_INT(g_fg_recorded, 1);
    CHECK_INT(uscale_retire_frame(ctx, 42), USCALE_OK);
    CHECK_INT(g_fg_retired, 42);
    uscale_context_destroy(ctx);
    CHECK_INT(g_live, 0);
    CHECK_INT(g_fg_destroyed, 1);

    /* Frame generation fails to start: create fails and frees the backend. */
    reset();
    g_fg_fail = 1;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_ERROR_VULKAN);
    CHECK(ctx == NULL);
    CHECK_INT(g_live, 0);

    /* Frame generation off: record is unsupported, retire is a no-op. */
    reset();
    info.enable_frame_generation = 0;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(uscale_record_framegen(ctx, &fg), USCALE_ERROR_UNSUPPORTED);
    CHECK_INT(uscale_retire_frame(ctx, 1), USCALE_OK);
    uscale_context_destroy(ctx);
    CHECK_INT(g_live, 0);

    /* Bad arguments. */
    reset();
    ctx = NULL;
    CHECK_INT(uscale_context_create(NULL, &ctx), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(uscale_context_create(&info, NULL), USCALE_ERROR_INVALID_ARGUMENT);
    info.struct_size = 4;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_ERROR_VERSION_MISMATCH);
    info = make_info(USCALE_BACKEND_AUTO);
    info.api_version = USCALE_MAKE_VERSION(USCALE_VERSION_MAJOR + 1, 0, 0);
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_ERROR_VERSION_MISMATCH);
    CHECK_INT(uscale_record_upscale(NULL, &up), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(uscale_record_framegen(NULL, &fg), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(uscale_retire_frame(NULL, 1), USCALE_ERROR_INVALID_ARGUMENT);
    uscale_context_destroy(NULL);
    CHECK_INT(uscale_context_backend(NULL, &reason), USCALE_BACKEND_AUTO);
    CHECK_INT(g_live, 0);

    uscale_context_set_hooks(NULL);
    USCALE_TEST_END;
}
