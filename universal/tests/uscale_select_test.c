/* Locked test for roadmap 2.03 (backend choice and fallback). Do not edit.
 * SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "uscale.h"
#include "uscale_test.h"

#define CAPS_SIZE ((uint32_t)sizeof(uscale_device_caps))

/* struct_size, vendor, int_dot, fp16, moltenvk, fsr4_assets, xess_rt, dlss_rt */
static const uscale_device_caps k_rtx_all = {CAPS_SIZE, USCALE_VENDOR_NVIDIA, 1, 1, 0, 1, 1, 1};
static const uscale_device_caps k_amd_fsr4 = {CAPS_SIZE, USCALE_VENDOR_AMD, 1, 1, 0, 1, 1, 0};
static const uscale_device_caps k_amd_xess = {CAPS_SIZE, USCALE_VENDOR_AMD, 1, 1, 0, 0, 1, 0};
static const uscale_device_caps k_old_gpu = {CAPS_SIZE, USCALE_VENDOR_AMD, 0, 0, 0, 0, 0, 0};
static const uscale_device_caps k_mac = {CAPS_SIZE, USCALE_VENDOR_AMD, 1, 1, 1, 0, 1, 1};
static const uscale_device_caps k_amd_dlss_rt = {CAPS_SIZE, USCALE_VENDOR_AMD, 1, 1, 0, 0, 0, 1};
static const uscale_device_caps k_rtx_no_rt = {CAPS_SIZE, USCALE_VENDOR_NVIDIA, 1, 1, 0, 0, 0, 0};
static const uscale_device_caps k_assets_no_dot = {CAPS_SIZE, USCALE_VENDOR_AMD, 0, 1, 0, 1, 0, 0};
static const uscale_device_caps k_assets_no_fp16 = {CAPS_SIZE, USCALE_VENDOR_AMD, 1, 0, 0, 1, 0, 0};
static const uscale_device_caps k_xess_no_dot = {CAPS_SIZE, USCALE_VENDOR_INTEL, 0, 1, 0, 0, 1, 0};
static const uscale_device_caps k_truthy = {CAPS_SIZE, USCALE_VENDOR_NVIDIA, 2, 2, 0, 0, 0, 7};
static const uscale_device_caps k_old_struct = {CAPS_SIZE - 4u, USCALE_VENDOR_NVIDIA, 1, 1, 0, 1, 1, 1};

static void expect(const uscale_device_caps *caps, uscale_backend preferred,
                   uscale_backend want, const char *reason)
{
    uscale_backend got = USCALE_BACKEND_COUNT;
    const char *why = NULL;
    CHECK_INT(uscale_select_backend(caps, preferred, &got, &why), USCALE_OK);
    CHECK_INT(got, want);
    CHECK_STR(why, reason);
}

int main(void)
{
    uscale_backend got = USCALE_BACKEND_COUNT;
    const char *why = "untouched";

    /* Expectation 1: NULL caps or NULL out_backend is invalid; outputs untouched. */
    CHECK_INT(uscale_select_backend(NULL, USCALE_BACKEND_AUTO, &got, &why), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(uscale_select_backend(&k_rtx_all, USCALE_BACKEND_AUTO, NULL, &why), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(got, USCALE_BACKEND_COUNT);
    CHECK_STR(why, "untouched");

    /* Expectation 2: struct_size smaller than the struct is a version mismatch. */
    CHECK_INT(uscale_select_backend(&k_old_struct, USCALE_BACKEND_AUTO, &got, &why), USCALE_ERROR_VERSION_MISMATCH);

    /* Expectation 3: preferred outside 0..4 is invalid. */
    CHECK_INT(uscale_select_backend(&k_rtx_all, USCALE_BACKEND_COUNT, &got, &why), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(uscale_select_backend(&k_rtx_all, (uscale_backend)-1, &got, &why), USCALE_ERROR_INVALID_ARGUMENT);
    CHECK_INT(got, USCALE_BACKEND_COUNT);

    /* Expectation 4: AUTO takes the first available of DLSS, FSR 4, XeSS, FSR 3. */
    expect(&k_rtx_all, USCALE_BACKEND_AUTO, USCALE_BACKEND_DLSS, "auto: DLSS is the best available");
    expect(&k_amd_fsr4, USCALE_BACKEND_AUTO, USCALE_BACKEND_FSR4, "auto: FSR 4 is the best available");
    expect(&k_amd_xess, USCALE_BACKEND_AUTO, USCALE_BACKEND_XESS, "auto: XeSS is the best available");
    expect(&k_old_gpu, USCALE_BACKEND_AUTO, USCALE_BACKEND_FSR3, "auto: FSR 3 is the best available");
    expect(&k_mac, USCALE_BACKEND_AUTO, USCALE_BACKEND_FSR3, "auto: FSR 3 is the best available");
    expect(&k_amd_dlss_rt, USCALE_BACKEND_AUTO, USCALE_BACKEND_FSR3, "auto: FSR 3 is the best available");

    /* Expectation 5: an available requested backend is used. */
    expect(&k_rtx_all, USCALE_BACKEND_XESS, USCALE_BACKEND_XESS, "requested: XeSS");
    expect(&k_rtx_all, USCALE_BACKEND_FSR4, USCALE_BACKEND_FSR4, "requested: FSR 4");
    expect(&k_rtx_all, USCALE_BACKEND_DLSS, USCALE_BACKEND_DLSS, "requested: DLSS");
    expect(&k_old_gpu, USCALE_BACKEND_FSR3, USCALE_BACKEND_FSR3, "requested: FSR 3");

    /* Expectation 6: an unavailable request falls back to FSR 3 with the first failing reason. */
    expect(&k_old_gpu, USCALE_BACKEND_FSR4, USCALE_BACKEND_FSR3, "FSR 4 unavailable: no FSR 4 assets");
    expect(&k_assets_no_dot, USCALE_BACKEND_FSR4, USCALE_BACKEND_FSR3, "FSR 4 unavailable: no integer dot product");
    expect(&k_assets_no_fp16, USCALE_BACKEND_FSR4, USCALE_BACKEND_FSR3, "FSR 4 unavailable: no FP16");
    expect(&k_mac, USCALE_BACKEND_XESS, USCALE_BACKEND_FSR3, "XeSS unavailable: no XeSS runtime on MoltenVK");
    expect(&k_old_gpu, USCALE_BACKEND_XESS, USCALE_BACKEND_FSR3, "XeSS unavailable: XeSS runtime not found");
    expect(&k_xess_no_dot, USCALE_BACKEND_XESS, USCALE_BACKEND_FSR3, "XeSS unavailable: no integer dot product");
    expect(&k_mac, USCALE_BACKEND_DLSS, USCALE_BACKEND_FSR3, "DLSS unavailable: no DLSS runtime on MoltenVK");
    expect(&k_amd_dlss_rt, USCALE_BACKEND_DLSS, USCALE_BACKEND_FSR3, "DLSS unavailable: not an NVIDIA GPU");
    expect(&k_rtx_no_rt, USCALE_BACKEND_DLSS, USCALE_BACKEND_FSR3, "DLSS unavailable: DLSS runtime not found");

    /* Expectation 7: any non-zero flag counts as true. */
    expect(&k_truthy, USCALE_BACKEND_AUTO, USCALE_BACKEND_DLSS, "auto: DLSS is the best available");

    /* Expectation 8: out_reason may be NULL. */
    got = USCALE_BACKEND_COUNT;
    CHECK_INT(uscale_select_backend(&k_amd_fsr4, USCALE_BACKEND_AUTO, &got, NULL), USCALE_OK);
    CHECK_INT(got, USCALE_BACKEND_FSR4);

    USCALE_TEST_END;
}
