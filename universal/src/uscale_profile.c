/* uscale_profile_flags_from_env: present (code map: universal/src, planned). SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "uscale.h"

/* Same rule as upstream's cc76692 switches: on only when the value is non-NULL
 * and its first character is '1'. No trimming, parsing or whole-string compare. */
static uint32_t uscale_profile_switch_on(const char *v)
{
    return (v != NULL && v[0] == '1') ? 1u : 0u;
}

uint32_t uscale_profile_flags_from_env(const char *bb_fsr4_profile, const char *bb_fsr4_stats)
{
    uint32_t flags = 0u;

    if (uscale_profile_switch_on(bb_fsr4_profile)) {
        flags |= USCALE_PROFILE_GPU_TIMES;
    }
    if (uscale_profile_switch_on(bb_fsr4_stats)) {
        flags |= USCALE_PROFILE_PIPELINE_STATS;
    }
    return flags;
}
