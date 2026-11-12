/* Tiny consumer project testing find_package(uscale). SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <uscale.h>

int main(void)
{
    uint32_t ver = uscale_version();
    printf("uscale consumer: version=0x%08x\n", (unsigned int)ver);
    return ver == USCALE_VERSION ? 0 : 1;
}
