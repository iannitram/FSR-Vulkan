/* uscale_query_caps: present (code map: universal/src, roadmap 3.01, 5.04, 5.06). Reports the XeSS and
 * DLSS runtimes when found, 0 otherwise. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "runtime_loader.h"
#include "uscale_vulkan.h"

#define USCALE_EXT_DOT_PRODUCT "VK_KHR_shader_integer_dot_product"
#define USCALE_EXT_FLOAT16 "VK_KHR_shader_float16_int8"
#define USCALE_EXT_PORTABILITY "VK_KHR_portability_subset" /* MoltenVK advertises it */

#if defined(_WIN32)
static const char *const k_xess_runtime_names[] = {"libxess.dll"};
#elif defined(__APPLE__)
static const char *const k_xess_runtime_names[] = {"libxess.dylib", "libxess.so"};
#else
static const char *const k_xess_runtime_names[] = {"libxess.so", "libxess.so.1"};
#endif

/* Internal seam used by the runtime-free test. A NULL directory searches the
 * process's normal library locations; a non-NULL directory restricts the scan. */
uint32_t uscale_dlss_runtime_in_dir(const char *directory);
#if !defined(_WIN32)
void uscale_dlss_path_list(char *list, size_t cap, const char *exe_dir, const char *ld_path);
uint32_t uscale_dlss_scan_path_list(const char *list);
#endif
uint32_t uscale_dlss_runtime_available(uint32_t vendor_id, uint32_t is_moltenvk,
                                       const char *directory);

#if !defined(_WIN32)
/* Linux search order: the folder beside the executable, each LD_LIBRARY_PATH entry, then the
 * standard library folders. Empty entries are dropped (the loader reads them as the working
 * directory, which a caps query should not scan). A piece that does not fit is skipped. */
static void path_list_add(char *list, size_t cap, const char *piece)
{
    size_t len = strlen(list), add = piece != NULL ? strlen(piece) : 0;
    if (add == 0 || len + (len > 0) + add >= cap) return;
    if (len > 0) list[len++] = ':';
    memcpy(list + len, piece, add + 1);
}

void uscale_dlss_path_list(char *list, size_t cap, const char *exe_dir, const char *ld_path)
{
    if (cap == 0) return;
    list[0] = '\0';
    path_list_add(list, cap, exe_dir);
    path_list_add(list, cap, ld_path);
    path_list_add(list, cap, "/usr/lib:/usr/lib64:/usr/lib/x86_64-linux-gnu:"
        "/usr/lib/aarch64-linux-gnu:/lib:/lib64:/lib/x86_64-linux-gnu:"
        "/lib/aarch64-linux-gnu:/usr/local/lib");
}

static uint32_t dlss_scan_dir(const char *dirname)
{
    DIR *dir = opendir(dirname);
    struct dirent *entry;
    if (dir == NULL) return 0;
    while ((entry = readdir(dir)) != NULL) {
        char path[4096];
        const char prefix[] = "libnvidia-ngx-dlss.so.";
        const char *const names[] = { path };
        uscale_runtime *runtime = NULL;
        int n;
        if (strncmp(entry->d_name, prefix, sizeof(prefix) - 1) != 0 ||
            entry->d_name[sizeof(prefix) - 1] == '\0') continue;
        n = snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);
        if (n <= 0 || (size_t)n >= sizeof(path)) continue;
        if (uscale_runtime_open(names, 1, &runtime) == USCALE_OK) {
            uscale_runtime_close(runtime);
            closedir(dir);
            return 1;
        }
    }
    closedir(dir);
    return 0;
}

uint32_t uscale_dlss_scan_path_list(const char *list)
{
    while (*list != '\0') {
        size_t len = strcspn(list, ":");
        char dirname[4096];
        if (len > 0 && len < sizeof(dirname)) {
            memcpy(dirname, list, len);
            dirname[len] = '\0';
            if (dlss_scan_dir(dirname)) return 1;
        }
        list += len + (list[len] == ':');
    }
    return 0;
}
#endif

uint32_t uscale_dlss_runtime_in_dir(const char *directory)
{
#if defined(_WIN32)
    char path[MAX_PATH];
    const char *name = "nvngx_dlss.dll";
    const char *names[] = { name };
    uscale_runtime *runtime = NULL;
    if (directory != NULL) {
        int n = snprintf(path, sizeof(path), "%s\\%s", directory, name);
        if (n <= 0 || (size_t)n >= sizeof(path)) return 0;
        names[0] = path;
    }
    if (uscale_runtime_open(names, 1, &runtime) != USCALE_OK) return 0;
    uscale_runtime_close(runtime);
    return 1;
#else
    char list[8192];
    char *exe_dir = NULL;
#if defined(__linux__)
    char exe[4096];
    ssize_t got = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (got > 0) {
        exe[got] = '\0';
        exe_dir = strrchr(exe, '/');
        if (exe_dir != NULL) { *exe_dir = '\0'; exe_dir = exe; }
    }
#endif
    if (directory != NULL) return uscale_dlss_scan_path_list(directory);
    uscale_dlss_path_list(list, sizeof(list), exe_dir, getenv("LD_LIBRARY_PATH"));
    return uscale_dlss_scan_path_list(list);
#endif
}

uint32_t uscale_dlss_runtime_available(uint32_t vendor_id, uint32_t is_moltenvk,
                                       const char *directory)
{
    return vendor_id == 0x10DEu && !is_moltenvk &&
        uscale_dlss_runtime_in_dir(directory);
}

static int has_extension(const VkExtensionProperties *exts, uint32_t count, const char *name)
{
    uint32_t i;
    for (i = 0; i < count; ++i) {
        if (strcmp(exts[i].extensionName, name) == 0)
            return 1;
    }
    return 0;
}

/* A FSR 4 asset folder is valid when it exists and holds at least one entry. */
static uint32_t dir_has_entries(const char *dir)
{
#if defined(_WIN32)
    char pattern[MAX_PATH];
    WIN32_FIND_DATAA data;
    HANDLE find;
    uint32_t found = 0;
    int n = snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    if (n <= 0 || (size_t)n >= sizeof(pattern))
        return 0;
    find = FindFirstFileA(pattern, &data);
    if (find == INVALID_HANDLE_VALUE)
        return 0;
    do {
        if (strcmp(data.cFileName, ".") != 0 && strcmp(data.cFileName, "..") != 0) {
            found = 1;
            break;
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);
    return found;
#else
    DIR *d = opendir(dir);
    struct dirent *entry;
    uint32_t found = 0;
    if (d == NULL)
        return 0;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
#endif
}

uscale_result uscale_query_caps(VkPhysicalDevice physical_device,
                                PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                                VkInstance instance, const char *fsr4_asset_dir,
                                uscale_device_caps *out_caps)
{
    PFN_vkGetPhysicalDeviceFeatures2 get_features2;
    PFN_vkGetPhysicalDeviceProperties get_props;
    PFN_vkEnumerateDeviceExtensionProperties enum_exts;
    VkPhysicalDeviceProperties props;
    VkExtensionProperties *exts;
    uint32_t ext_count = 0;
    VkPhysicalDeviceShaderIntegerDotProductFeatures dot = {0};
    VkPhysicalDeviceShaderFloat16Int8Features f16 = {0};
    VkPhysicalDeviceFeatures2 features = {0};
    uscale_device_caps caps;
    int dot_ok, f16_ok;

    if (physical_device == VK_NULL_HANDLE || instance == VK_NULL_HANDLE ||
        get_instance_proc_addr == NULL || out_caps == NULL)
        return USCALE_ERROR_INVALID_ARGUMENT;

    get_features2 = (PFN_vkGetPhysicalDeviceFeatures2)
        get_instance_proc_addr(instance, "vkGetPhysicalDeviceFeatures2");
    if (get_features2 == NULL)
        get_features2 = (PFN_vkGetPhysicalDeviceFeatures2)
            get_instance_proc_addr(instance, "vkGetPhysicalDeviceFeatures2KHR");
    /* Properties and extensions load through get_instance_proc_addr too. */
    get_props = (PFN_vkGetPhysicalDeviceProperties)
        get_instance_proc_addr(instance, "vkGetPhysicalDeviceProperties");
    enum_exts = (PFN_vkEnumerateDeviceExtensionProperties)
        get_instance_proc_addr(instance, "vkEnumerateDeviceExtensionProperties");
    if (get_features2 == NULL || get_props == NULL || enum_exts == NULL)
        return USCALE_ERROR_UNSUPPORTED;

    get_props(physical_device, &props);
    if (enum_exts(physical_device, NULL, &ext_count, NULL) != VK_SUCCESS)
        return USCALE_ERROR_VULKAN;
    exts = calloc(ext_count > 0 ? ext_count : 1, sizeof(*exts));
    if (exts == NULL)
        return USCALE_ERROR_OUT_OF_MEMORY;
    if (enum_exts(physical_device, NULL, &ext_count, exts) != VK_SUCCESS) {
        free(exts);
        return USCALE_ERROR_VULKAN;
    }

    /* A feature counts when the extension is listed or the device's core version has it. */
    dot_ok = has_extension(exts, ext_count, USCALE_EXT_DOT_PRODUCT) ||
             props.apiVersion >= VK_API_VERSION_1_3;
    f16_ok = has_extension(exts, ext_count, USCALE_EXT_FLOAT16) ||
             props.apiVersion >= VK_API_VERSION_1_2;

    dot.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES;
    f16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    /* Chain a struct only when the device supports it; the spec forbids the rest. */
    if (f16_ok) {
        f16.pNext = features.pNext, features.pNext = &f16;
    }
    if (dot_ok) {
        dot.pNext = features.pNext, features.pNext = &dot;
    }
    get_features2(physical_device, &features);

    memset(&caps, 0, sizeof(caps));
    caps.struct_size = sizeof(caps);
    caps.vendor_id = props.vendorID;
    caps.has_int_dot_product = dot_ok && dot.shaderIntegerDotProduct == VK_TRUE;
    caps.has_fp16 = f16_ok && f16.shaderFloat16 == VK_TRUE;
    caps.is_moltenvk = has_extension(exts, ext_count, USCALE_EXT_PORTABILITY);
    caps.fsr4_assets_available = fsr4_asset_dir != NULL && dir_has_entries(fsr4_asset_dir);
    caps.xess_runtime_available = 0;
    if (!caps.is_moltenvk) {
        uscale_runtime *xess_runtime = NULL;
        if (uscale_runtime_open(k_xess_runtime_names,
                                (unsigned)(sizeof(k_xess_runtime_names) /
                                           sizeof(k_xess_runtime_names[0])),
                                &xess_runtime) == USCALE_OK) {
            caps.xess_runtime_available = 1;
            uscale_runtime_close(xess_runtime);
        }
    }

    caps.dlss_runtime_available = uscale_dlss_runtime_available(
        props.vendorID, caps.is_moltenvk, NULL);

    free(exts);
    *out_caps = caps;
    return USCALE_OK;
}
