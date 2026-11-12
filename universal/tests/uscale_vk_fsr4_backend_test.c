/* FSR 4 backend without a GPU: the no-asset fallback to FSR 3 with the 2.03 reason, the
 * capability gate with fake caps, the v07 file names inside the game's folder, and the loader
 * with made-up files written into a scratch folder (argv[1]) and removed again.
 * No asset file is committed; the real device create is the 7.01 check on a GPU.
 * SPDX-License-Identifier: MIT */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS /* MSVC /W4 /WX: fopen and getenv are fine here */
#endif
#include <stdio.h>
#include <stdlib.h>

#include "backend.h"
#include "backend_fsr4.h"
#include "uscale_test.h"

static uscale_device_caps g_caps;
static int g_fsr3_created;

static uscale_result fake_fsr3_create(const uscale_context_create_info *i, void **o)
{
    (void)i;
    g_fsr3_created++;
    *o = malloc(1);
    return *o != NULL ? USCALE_OK : USCALE_ERROR_OUT_OF_MEMORY;
}
static uscale_result fake_upscale(void *s, const uscale_upscale_info *i)
{
    (void)s;
    (void)i;
    return USCALE_OK;
}
static void fake_destroy(void *s)
{
    free(s);
}
static const uscale_backend_ops k_fake_fsr3 = {USCALE_BACKEND_FSR3, fake_fsr3_create, fake_upscale,
                                               fake_destroy};

/* FSR 4 is the real backend; FSR 3 is fake so no GPU is needed. */
static const uscale_backend_ops *ops_for(uscale_backend id)
{
    return id == USCALE_BACKEND_FSR3 ? &k_fake_fsr3 : id == USCALE_BACKEND_FSR4
               ? uscale_backend_fsr4_ops() : NULL;
}
static uscale_result fake_caps(const uscale_context_create_info *i, uscale_device_caps *o)
{
    (void)i;
    *o = g_caps;
    return USCALE_OK;
}
static const uscale_framegen_ops k_no_fg = {0};
static const uscale_context_hooks k_hooks = {ops_for, fake_caps, &k_no_fg};

static void test_no_assets_falls_back(void)
{
    uscale_context_create_info info = {0};
    uscale_context *ctx = NULL;
    const char *reason = NULL;

    g_caps.struct_size = sizeof(g_caps);
    g_caps.has_int_dot_product = 1;
    g_caps.has_fp16 = 1;
    g_caps.fsr4_assets_available = 0; /* the game gave no asset folder */
    info.struct_size = sizeof(info);
    info.api_version = USCALE_VERSION;
    info.preferred_backend = USCALE_BACKEND_FSR4;
    info.fsr4_asset_dir = NULL;
    g_fsr3_created = 0;
    uscale_context_set_hooks(&k_hooks);
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(uscale_context_backend(ctx, &reason), USCALE_BACKEND_FSR3);
    CHECK_STR(reason, "FSR 4 unavailable: no FSR 4 assets");
    CHECK_INT(g_fsr3_created, 1);
    uscale_context_destroy(ctx);

    /* Auto picks FSR 3 too, with the same reason in the message the selector owns. */
    info.preferred_backend = USCALE_BACKEND_AUTO;
    CHECK_INT(uscale_context_create(&info, &ctx), USCALE_OK);
    CHECK_INT(uscale_context_backend(ctx, &reason), USCALE_BACKEND_FSR3);
    uscale_context_destroy(ctx);
    uscale_context_set_hooks(NULL);
}

static void test_caps_gate(void)
{
    uscale_device_caps c = {0};
    const char *reason = NULL;

    c.struct_size = sizeof(c);
    c.has_int_dot_product = 1;
    c.has_fp16 = 1;
    CHECK_INT(uscale_fsr4_check_caps(&c, &reason), USCALE_OK);
    CHECK(reason == NULL);
    c.has_fp16 = 0;
    CHECK_INT(uscale_fsr4_check_caps(&c, &reason), USCALE_ERROR_UNSUPPORTED);
    CHECK_STR(reason, "FSR 4 unavailable: no FP16");
    c.has_fp16 = 1;
    c.has_int_dot_product = 0;
    CHECK_INT(uscale_fsr4_check_caps(&c, &reason), USCALE_ERROR_UNSUPPORTED);
    CHECK_STR(reason, "FSR 4 unavailable: no integer dot product");
    c.has_fp16 = 0;
    CHECK_INT(uscale_fsr4_check_caps(&c, NULL), USCALE_ERROR_UNSUPPORTED);
    CHECK_INT(uscale_fsr4_check_caps(NULL, &reason), USCALE_ERROR_INVALID_ARGUMENT);
}

/* With caps good and no folder, the real create stops before any device call. */
static void test_create_without_folder(void)
{
    uscale_context_create_info info = {0};
    void *state = (void *)&info;
    const uscale_backend_ops *ops = uscale_backend_fsr4_ops();

    CHECK_INT(ops->id, USCALE_BACKEND_FSR4);
    info.struct_size = sizeof(info);
    CHECK_INT(ops->create(&info, &state), USCALE_ERROR_INVALID_ARGUMENT); /* no device */
    CHECK(state == NULL);
}

static int write_file(const char *path, size_t size)
{
    FILE *f = fopen(path, "wb");
    size_t i;

    if (f == NULL)
        return 0;
    for (i = 0; i < size; i++)
        fputc((int)(i & 0xff), f);
    return fclose(f) == 0;
}

static void test_paths_and_loader(const char *dir)
{
    uscale_fsr4_paths *p = (uscale_fsr4_paths *)calloc(1, sizeof(*p));
    uscale_fsr4_assets a;
    uscale_extent d4k = {3840, 2160};
    uscale_extent zero = {0, 0};
    unsigned i;
    char *tmp = (char *)malloc(USCALE_FSR4_PATH_MAX);

    if (p == NULL || tmp == NULL) {
        CHECK(0);
        return;
    }
    CHECK_INT(uscale_fsr4_build_paths(NULL, USCALE_QUALITY_QUALITY, d4k, p),
              USCALE_ERROR_RUNTIME_MISSING);
    CHECK_INT(uscale_fsr4_build_paths("", USCALE_QUALITY_QUALITY, d4k, p),
              USCALE_ERROR_RUNTIME_MISSING);
    CHECK_INT(uscale_fsr4_build_paths(dir, USCALE_QUALITY_COUNT, d4k, p),
              USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(uscale_fsr4_build_paths(dir, USCALE_QUALITY_QUALITY, zero, p), USCALE_ERROR_UNSUPPORTED);
    CHECK_INT(uscale_fsr4_build_paths(dir, USCALE_QUALITY_QUALITY, d4k, p), USCALE_OK);

    /* Every name sits inside the game's folder and names a v07 file. */
    for (i = 0; i < USCALE_FSR4_PASS_COUNT; i++) {
        CHECK(strncmp(p->shader[i], dir, strlen(dir)) == 0);
        CHECK(strstr(p->shader[i], "fsr4_model_v07") != NULL || strstr(p->shader[i], ".spv") != NULL);
    }
    CHECK(strstr(p->initializer, "_initializers.bin") != NULL);
    CHECK(strstr(p->pre_weights, "_pre_weights.bin") != NULL);

    /* Nothing there yet: the first missing file is reported as missing assets. */
    CHECK_INT(uscale_fsr4_load_assets(p, &a), USCALE_ERROR_RUNTIME_MISSING);

    /* Made-up bytes with the right sizes load; they are removed below. */
    for (i = 0; i < USCALE_FSR4_PASS_COUNT; i++)
        CHECK(write_file(p->shader[i], 64));
    CHECK(write_file(p->initializer, 89216));
    CHECK(write_file(p->pre_weights, 1023)); /* wrong size */
    CHECK_INT(uscale_fsr4_load_assets(p, &a), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK(write_file(p->pre_weights, 1024));
    CHECK_INT(uscale_fsr4_load_assets(p, &a), USCALE_OK);
    CHECK_INT(a.shader_size[0], 64);
    CHECK_INT(a.initializer_size, 89216);
    CHECK_INT(a.pre_weights_size, 1024);
    uscale_fsr4_assets_free(&a);
    CHECK(a.initializer == NULL);

    CHECK(write_file(p->shader[5], 62)); /* not whole words */
    CHECK_INT(uscale_fsr4_load_assets(p, &a), USCALE_ERROR_INVALID_ARGUMENT);

    for (i = 0; i < USCALE_FSR4_PASS_COUNT; i++)
        remove(p->shader[i]);
    remove(p->initializer);
    remove(p->pre_weights);
    free(tmp);
    free(p);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <scratch folder>\n", argv[0]);
        return 1;
    }
    test_no_assets_falls_back();
    test_caps_gate();
    test_create_without_folder();
    test_paths_and_loader(argv[1]);
    USCALE_TEST_END;
}
