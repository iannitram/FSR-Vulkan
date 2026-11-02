/* The Vulkan half links and its stubs answer (SPEC.md M1). Replaced by real
 * context tests as the backends land. SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "uscale_vulkan.h"
#include "uscale_test.h"

int main(void)
{
    uscale_context *ctx = (uscale_context *)0x1;
    uscale_result r = uscale_context_create(NULL, &ctx);
    CHECK(r != USCALE_OK);
    CHECK(ctx == NULL);
    uscale_context_destroy(NULL);
    USCALE_TEST_END;
}
