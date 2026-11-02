/* SPEC.md M2: the linked library reports the header's version. SPDX-License-Identifier: MIT */
#include "uscale.h"
#include "uscale_test.h"

int main(void)
{
    CHECK_INT(uscale_version(), USCALE_VERSION);
    CHECK_INT(USCALE_MAKE_VERSION(1, 2, 3), (1u << 22) | (2u << 12) | 3u);
    CHECK_INT(uscale_version() >> 22, USCALE_VERSION_MAJOR);
    CHECK_INT((uscale_version() >> 12) & 0x3FFu, USCALE_VERSION_MINOR);
    CHECK_INT(uscale_version() & 0xFFFu, USCALE_VERSION_PATCH);
    USCALE_TEST_END;
}
