/* Loads a vendor runtime (XeSS, DLSS) from the player's machine at run time:
 * dlopen on Linux and macOS, LoadLibrary on Windows. We ship no vendor files.
 * SPDX-License-Identifier: MIT */
#ifndef USCALE_RUNTIME_LOADER_H
#define USCALE_RUNTIME_LOADER_H

typedef struct uscale_runtime uscale_runtime;

/* Tries each name in turn; returns NULL when none loads. */
uscale_runtime *uscale_runtime_open(const char *const *names, unsigned count);
void *uscale_runtime_symbol(uscale_runtime *runtime, const char *name);
void uscale_runtime_close(uscale_runtime *runtime);

#endif
