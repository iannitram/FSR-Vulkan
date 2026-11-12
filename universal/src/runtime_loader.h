/* Loads a vendor runtime (XeSS, DLSS) from the player's machine at run time:
 * dlopen on Linux and macOS, LoadLibrary on Windows. We ship no vendor files.
 * SPDX-License-Identifier: MIT */
#ifndef USCALE_RUNTIME_LOADER_H
#define USCALE_RUNTIME_LOADER_H

#include "uscale.h"

typedef struct uscale_runtime uscale_runtime;

/* Tries names in order and owns the first library that loads. Names must be
 * non-NULL, nonempty strings and count must be positive. out_runtime is required
 * and is set to NULL on failure. Returns USCALE_OK, USCALE_ERROR_RUNTIME_MISSING
 * when none loads, USCALE_ERROR_INVALID_ARGUMENT, or USCALE_ERROR_OUT_OF_MEMORY.
 * Close each successful open, even when the same library is opened again. */
uscale_result uscale_runtime_open(const char *const *names, unsigned count,
                                  uscale_runtime **out_runtime);
/* Returns NULL for an absent symbol or invalid argument. Symbol addresses are
 * valid until close; callers convert them to the symbol's actual pointer type. */
void *uscale_runtime_symbol(uscale_runtime *runtime, const char *name);
/* Releases the library and its handle; NULL is a no-op. */
void uscale_runtime_close(uscale_runtime *runtime);

#endif
