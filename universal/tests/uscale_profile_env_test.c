/* Locked test for roadmap 2.04 (profiling flags from the environment). Do not edit.
 * SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "uscale.h"
#include "uscale_test.h"

int main(void)
{
    /* Expectation 1: both unset gives 0. */
    CHECK_INT(uscale_profile_flags_from_env(NULL, NULL), 0u);

    /* Expectation 2: a value is on only when its first character is '1'. */
    CHECK_INT(uscale_profile_flags_from_env("1", NULL), USCALE_PROFILE_GPU_TIMES);
    CHECK_INT(uscale_profile_flags_from_env("1yes", NULL), USCALE_PROFILE_GPU_TIMES);
    CHECK_INT(uscale_profile_flags_from_env("0", NULL), 0u);
    CHECK_INT(uscale_profile_flags_from_env("", NULL), 0u);
    CHECK_INT(uscale_profile_flags_from_env("true", NULL), 0u);
    CHECK_INT(uscale_profile_flags_from_env(" 1", NULL), 0u);
    CHECK_INT(uscale_profile_flags_from_env("2", NULL), 0u);

    /* Expectation 3: BB_FSR4_PROFILE sets 0x1, BB_FSR4_STATS sets 0x2. */
    CHECK_INT(uscale_profile_flags_from_env(NULL, "1"), USCALE_PROFILE_PIPELINE_STATS);
    CHECK_INT(uscale_profile_flags_from_env("1", "1"),
              USCALE_PROFILE_GPU_TIMES | USCALE_PROFILE_PIPELINE_STATS);
    CHECK_INT(uscale_profile_flags_from_env("0", "1"), USCALE_PROFILE_PIPELINE_STATS);
    CHECK_INT(uscale_profile_flags_from_env("1", "off"), USCALE_PROFILE_GPU_TIMES);

    /* Expectation 4: the bit values are fixed. */
    CHECK_INT(USCALE_PROFILE_GPU_TIMES, 0x1u);
    CHECK_INT(USCALE_PROFILE_PIPELINE_STATS, 0x2u);

    USCALE_TEST_END;
}
