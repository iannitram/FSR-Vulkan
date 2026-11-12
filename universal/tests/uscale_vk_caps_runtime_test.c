/* XeSS runtime discovery and caps reporting without a GPU or vendor binary. SPDX-License-Identifier: MIT */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "uscale_vulkan.h"
#include "uscale_test.h"

#if defined(_WIN32)
#define TEST_XESS_NAME "libxess.dll"
#elif defined(__APPLE__)
#define TEST_XESS_NAME "libxess.dylib"
#define TEST_XESS_FALLBACK_NAME "libxess.so"
#else
#define TEST_XESS_NAME "libxess.so"
#define TEST_XESS_FALLBACK_NAME "libxess.so.1"
#endif

static int g_mock_moltenvk = 0;

static void VKAPI_CALL mock_get_props(VkPhysicalDevice physicalDevice,
                                      VkPhysicalDeviceProperties *pProperties)
{
    (void)physicalDevice;
    memset(pProperties, 0, sizeof(*pProperties));
    pProperties->apiVersion = VK_API_VERSION_1_3;
    pProperties->vendorID = USCALE_VENDOR_INTEL;
}

static VkResult VKAPI_CALL mock_enum_exts(VkPhysicalDevice physicalDevice,
                                          const char *pLayerName,
                                          uint32_t *pPropertyCount,
                                          VkExtensionProperties *pProperties)
{
    (void)physicalDevice;
    (void)pLayerName;
    if (pProperties == NULL) {
        *pPropertyCount = g_mock_moltenvk ? 1u : 0u;
        return VK_SUCCESS;
    }
    if (g_mock_moltenvk) {
        if (*pPropertyCount > 0u) {
            memset(pProperties, 0, sizeof(*pProperties));
            strncpy(pProperties[0].extensionName, "VK_KHR_portability_subset",
                    sizeof(pProperties[0].extensionName) - 1u);
            pProperties[0].specVersion = 1u;
            *pPropertyCount = 1u;
        }
    } else {
        *pPropertyCount = 0u;
    }
    return VK_SUCCESS;
}

static void VKAPI_CALL mock_get_features2(VkPhysicalDevice physicalDevice,
                                          VkPhysicalDeviceFeatures2 *pFeatures)
{
    (void)physicalDevice;
    (void)pFeatures;
}

static PFN_vkVoidFunction VKAPI_CALL mock_get_proc_addr(VkInstance instance,
                                                        const char *pName)
{
    (void)instance;
    if (pName == NULL)
        return NULL;
    if (strcmp(pName, "vkGetPhysicalDeviceFeatures2") == 0 ||
        strcmp(pName, "vkGetPhysicalDeviceFeatures2KHR") == 0) {
        PFN_vkGetPhysicalDeviceFeatures2 fn = mock_get_features2;
        PFN_vkVoidFunction result = NULL;
        memcpy(&result, &fn, sizeof(result));
        return result;
    }
    if (strcmp(pName, "vkGetPhysicalDeviceProperties") == 0) {
        PFN_vkGetPhysicalDeviceProperties fn = mock_get_props;
        PFN_vkVoidFunction result = NULL;
        memcpy(&result, &fn, sizeof(result));
        return result;
    }
    if (strcmp(pName, "vkEnumerateDeviceExtensionProperties") == 0) {
        PFN_vkEnumerateDeviceExtensionProperties fn = mock_enum_exts;
        PFN_vkVoidFunction result = NULL;
        memcpy(&result, &fn, sizeof(result));
        return result;
    }
    return NULL;
}

static int copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buf[4096];
    size_t n;
    if (in == NULL)
        return -1;
    out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    if (fclose(out) != 0)
        return -1;
    return 0;
}

int main(int argc, char **argv)
{
    static char fake_device_storage;
    static char fake_instance_storage;
    VkPhysicalDevice fake_physical = (VkPhysicalDevice)&fake_device_storage;
    VkInstance fake_instance = (VkInstance)&fake_instance_storage;
    char target_path[1024];
    uscale_device_caps caps;
    const char *fixture_path;
    const char *temp_dir;

    CHECK_INT(argc, 3);
    if (argc < 3)
        return 1;
    fixture_path = argv[1];
    temp_dir = argv[2];

#if defined(_WIN32)
    SetDllDirectoryA(temp_dir);
#else
    {
        const char *dyld = getenv("DYLD_LIBRARY_PATH");
        const char *ld = getenv("LD_LIBRARY_PATH");
        int has_search = (dyld != NULL && strstr(dyld, temp_dir) != NULL) ||
                         (ld != NULL && strstr(ld, temp_dir) != NULL);
        if (!has_search && getenv("USCALE_TEST_SEARCH_SET") == NULL) {
            setenv("USCALE_TEST_SEARCH_SET", "1", 1);
            setenv("DYLD_LIBRARY_PATH", temp_dir, 1);
            setenv("LD_LIBRARY_PATH", temp_dir, 1);
            execv(argv[0], argv);
            perror("execv");
            return 1;
        }
    }
#endif

    snprintf(target_path, sizeof(target_path), "%s/%s", temp_dir, TEST_XESS_NAME);
    remove(target_path);
#if defined(TEST_XESS_FALLBACK_NAME)
    {
        char fallback_path[1024];
        snprintf(fallback_path, sizeof(fallback_path), "%s/%s", temp_dir, TEST_XESS_FALLBACK_NAME);
        remove(fallback_path);
    }
#endif

    /* 1. Without runtime on search path: caps report 0. */
    g_mock_moltenvk = 0;
    CHECK_INT(uscale_query_caps(fake_physical, mock_get_proc_addr, fake_instance, NULL, &caps),
              USCALE_OK);
    CHECK_INT(caps.struct_size, sizeof(caps));
    CHECK_INT(caps.vendor_id, USCALE_VENDOR_INTEL);
    CHECK_INT(caps.is_moltenvk, 0);
    CHECK_INT(caps.xess_runtime_available, 0);
    CHECK_INT(caps.dlss_runtime_available, 0);

    /* 2. With fake runtime on search path: caps report 1. */
    CHECK_INT(copy_file(fixture_path, target_path), 0);
    CHECK_INT(uscale_query_caps(fake_physical, mock_get_proc_addr, fake_instance, NULL, &caps),
              USCALE_OK);
    CHECK_INT(caps.is_moltenvk, 0);
    CHECK_INT(caps.xess_runtime_available, 1);
    CHECK_INT(caps.dlss_runtime_available, 0);

    /* 3. On MoltenVK: caps always report 0 even when the library is present. */
    g_mock_moltenvk = 1;
    CHECK_INT(uscale_query_caps(fake_physical, mock_get_proc_addr, fake_instance, NULL, &caps),
              USCALE_OK);
    CHECK_INT(caps.is_moltenvk, 1);
    CHECK_INT(caps.xess_runtime_available, 0);

    /* 4. After removing runtime from search path: caps report 0 again. */
    remove(target_path);
    g_mock_moltenvk = 0;
    CHECK_INT(uscale_query_caps(fake_physical, mock_get_proc_addr, fake_instance, NULL, &caps),
              USCALE_OK);
    CHECK_INT(caps.is_moltenvk, 0);
    CHECK_INT(caps.xess_runtime_available, 0);

#if defined(TEST_XESS_FALLBACK_NAME)
    /* 5. With fallback runtime name on search path: caps report 1. */
    {
        char fallback_path[1024];
        snprintf(fallback_path, sizeof(fallback_path), "%s/%s", temp_dir, TEST_XESS_FALLBACK_NAME);
        CHECK_INT(copy_file(fixture_path, fallback_path), 0);
        CHECK_INT(uscale_query_caps(fake_physical, mock_get_proc_addr, fake_instance, NULL, &caps),
                  USCALE_OK);
        CHECK_INT(caps.is_moltenvk, 0);
        CHECK_INT(caps.xess_runtime_available, 1);
        remove(fallback_path);
        CHECK_INT(uscale_query_caps(fake_physical, mock_get_proc_addr, fake_instance, NULL, &caps),
                  USCALE_OK);
        CHECK_INT(caps.xess_runtime_available, 0);
    }
#endif

    USCALE_TEST_END;
}
