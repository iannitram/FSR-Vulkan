/* Vendor runtime loader. SPDX-License-Identifier: MIT */
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
#include <string.h>
#else
#include <dlfcn.h>
#endif

#include "runtime_loader.h"

struct uscale_runtime {
#if defined(_WIN32)
    HMODULE handle;
#else
    void *handle;
#endif
};

uscale_result uscale_runtime_open(const char *const *names, unsigned count,
                                  uscale_runtime **out_runtime)
{
    uscale_runtime *runtime;
    unsigned i;
    if (out_runtime == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;
    *out_runtime = NULL;
    if (names == NULL || count == 0)
        return USCALE_ERROR_INVALID_ARGUMENT;
    for (i = 0; i < count; ++i) {
        if (names[i] == NULL || names[i][0] == '\0')
            return USCALE_ERROR_INVALID_ARGUMENT;
    }
    runtime = malloc(sizeof(*runtime));
    if (runtime == NULL)
        return USCALE_ERROR_OUT_OF_MEMORY;
    for (i = 0; i < count; ++i) {
#if defined(_WIN32)
        runtime->handle = LoadLibraryA(names[i]);
#else
        runtime->handle = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
#endif
        if (runtime->handle != NULL) {
            *out_runtime = runtime;
            return USCALE_OK;
        }
    }
    free(runtime);
    return USCALE_ERROR_RUNTIME_MISSING;
}

void *uscale_runtime_symbol(uscale_runtime *runtime, const char *name)
{
    if (runtime == NULL || name == NULL || name[0] == '\0')
        return NULL;
#if defined(_WIN32)
    {
        FARPROC proc = GetProcAddress(runtime->handle, name);
        void *symbol = NULL;
        /* Windows uses the same representation for these pointer types.
         * Copy it without a function/object pointer cast (MSVC /W4). */
        _Static_assert(sizeof(symbol) == sizeof(proc), "symbol pointer size");
        if (proc != NULL)
            memcpy(&symbol, &proc, sizeof(symbol));
        return symbol;
    }
#else
    return dlsym(runtime->handle, name);
#endif
}

void uscale_runtime_close(uscale_runtime *runtime)
{
    if (runtime == NULL)
        return;
#if defined(_WIN32)
    FreeLibrary(runtime->handle);
#else
    dlclose(runtime->handle);
#endif
    free(runtime);
}
