/* Private FSR 4 backend helpers, split out so tests can reach them without a GPU.
 * No FFX types here. SPDX-License-Identifier: MIT */
#ifndef USCALE_BACKEND_FSR4_H
#define USCALE_BACKEND_FSR4_H

#include <stddef.h>

#include "uscale_vulkan.h"

#define USCALE_FSR4_PASS_COUNT 16u     /* pre, 12 model passes, post, RCAS, SPD auto-exposure */
#define USCALE_FSR4_PATH_MAX 1024u

/* The v07 files the game supplies. Never shipped or committed by us. */
typedef struct uscale_fsr4_paths {
    char shader[USCALE_FSR4_PASS_COUNT][USCALE_FSR4_PATH_MAX];
    char initializer[USCALE_FSR4_PATH_MAX];
    char pre_weights[USCALE_FSR4_PATH_MAX];
} uscale_fsr4_paths;

/* The loaded bytes; owned by the struct, freed by uscale_fsr4_assets_free. */
typedef struct uscale_fsr4_assets {
    void *shader[USCALE_FSR4_PASS_COUNT];
    size_t shader_size[USCALE_FSR4_PASS_COUNT];
    void *initializer;
    size_t initializer_size;
    void *pre_weights;
    size_t pre_weights_size;
} uscale_fsr4_assets;

/* Capability gate (SPEC M6): integer dot product and FP16, in the same order and words as
 * uscale_select_backend. Returns USCALE_OK, or USCALE_ERROR_UNSUPPORTED with *out_reason set
 * (may be NULL). Needs no device. */
uscale_result uscale_fsr4_check_caps(const uscale_device_caps *caps, const char **out_reason);

/* Names the v07 files for a quality preset and display size inside asset_dir. Pure string
 * work, no file access. Needs FFX (upstream's name helper); without it: UNSUPPORTED.
 * NULL or empty dir: RUNTIME_MISSING. Too-long path: INVALID_ARGUMENT. */
uscale_result uscale_fsr4_build_paths(const char *asset_dir, uscale_quality quality,
                                      uscale_extent display, uscale_fsr4_paths *out);

/* Reads every file in paths. A missing or unreadable file: RUNTIME_MISSING. An initializer or
 * pre-pass weight file of the wrong size, or an empty / non-multiple-of-4 shader: INVALID_ARGUMENT.
 * On any error nothing is left allocated. */
uscale_result uscale_fsr4_load_assets(const uscale_fsr4_paths *paths, uscale_fsr4_assets *out);
void uscale_fsr4_assets_free(uscale_fsr4_assets *assets);

#endif
