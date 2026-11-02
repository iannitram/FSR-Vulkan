/* Vendor runtime loader: stub (code map: universal/src, planned). SPDX-License-Identifier: MIT */
#include <stddef.h>

#include "runtime_loader.h"

uscale_runtime *uscale_runtime_open(const char *const *names, unsigned count)
{
    (void)names;
    (void)count;
    return NULL;
}

void *uscale_runtime_symbol(uscale_runtime *runtime, const char *name)
{
    (void)runtime;
    (void)name;
    return NULL;
}

void uscale_runtime_close(uscale_runtime *runtime)
{
    (void)runtime;
}
