/* uscale_query_caps on a real device: vendor id and flags against what the device reports,
 * and the FSR 4 asset field for an empty, a missing and a present folder. Linux CI runs it on
 * Mesa lavapipe (vendor 0x10005); it skips with 77 when no device exists.
 * usage: uscale_vk_caps_test <empty dir> <present dir>
 * SPDX-License-Identifier: MIT */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdlib.h>

#include "uscale_vulkan.h"
#include "uscale_test.h"

#define LAVAPIPE_VENDOR_ID 0x10005u /* VK_VENDOR_ID_MESA */

static int unavailable(const char *reason, VkResult result)
{
    const char *required = getenv("USCALE_REQUIRE_VK_DEVICE");
    int fail = required != NULL && strcmp(required, "1") == 0;
    fprintf(stderr, "%s: %s (VkResult %d)\n",
            fail ? "FAIL" : "SKIP", reason, (int)result);
    return fail ? 1 : 77;
}

static uscale_result query(VkPhysicalDevice physical, VkInstance instance,
                           const char *asset_dir, uscale_device_caps *caps)
{
    memset(caps, 0, sizeof(*caps));
    return uscale_query_caps(physical, vkGetInstanceProcAddr, instance, asset_dir, caps);
}

static int has_device_extension(VkPhysicalDevice physical, const char *name)
{
    uint32_t count = 0, i;
    VkExtensionProperties *exts;
    int found = 0;
    vkEnumerateDeviceExtensionProperties(physical, NULL, &count, NULL);
    exts = calloc(count > 0 ? count : 1, sizeof(*exts));
    if (exts == NULL)
        return 0;
    if (vkEnumerateDeviceExtensionProperties(physical, NULL, &count, exts) == VK_SUCCESS) {
        for (i = 0; i < count; ++i) {
            if (strcmp(exts[i].extensionName, name) == 0)
                found = 1;
        }
    }
    free(exts);
    return found;
}

int main(int argc, char **argv)
{
    VkApplicationInfo app = {0};
    VkInstanceCreateInfo instance_info = {0};
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties;
    uscale_device_caps caps;
    uint32_t count = 1;
    VkResult result;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <empty dir> <present dir>\n", argv[0]);
        return 1;
    }

    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = VK_API_VERSION_1_1; /* vkGetPhysicalDeviceFeatures2 is core from 1.1 */
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app;
    result = vkCreateInstance(&instance_info, NULL, &instance);
    if (result != VK_SUCCESS)
        return unavailable("cannot create Vulkan instance", result);

    result = vkEnumeratePhysicalDevices(instance, &count, &physical);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        fprintf(stderr, "vkEnumeratePhysicalDevices failed: %d\n", (int)result);
        vkDestroyInstance(instance, NULL);
        return 1;
    }
    if (count == 0) {
        vkDestroyInstance(instance, NULL);
        return unavailable("no Vulkan physical device", result);
    }

    vkGetPhysicalDeviceProperties(physical, &properties);
    printf("Device: %s (vendor 0x%04x)\n", properties.deviceName, (unsigned)properties.vendorID);

    /* Vendor id and flags from the device. */
    CHECK_INT(query(physical, instance, NULL, &caps), USCALE_OK);
    CHECK_INT(caps.struct_size, sizeof(uscale_device_caps));
    CHECK_INT(caps.vendor_id, properties.vendorID);
    if (strstr(properties.deviceName, "llvmpipe") != NULL)
        CHECK_INT(caps.vendor_id, LAVAPIPE_VENDOR_ID);
    /* Dot product and FP16: the device's feature bit, counted when the extension is listed
     * or the core version has it (the same rule the library uses). */
    {
        VkPhysicalDeviceShaderIntegerDotProductFeatures dot = {0};
        VkPhysicalDeviceShaderFloat16Int8Features f16 = {0};
        VkPhysicalDeviceFeatures2 features = {0};
        int dot_expect, f16_expect;
        dot.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES;
        f16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &dot;
        dot.pNext = &f16;
        vkGetPhysicalDeviceFeatures2(physical, &features);
        dot_expect = dot.shaderIntegerDotProduct == VK_TRUE &&
            (has_device_extension(physical, "VK_KHR_shader_integer_dot_product") ||
             properties.apiVersion >= VK_API_VERSION_1_3);
        f16_expect = f16.shaderFloat16 == VK_TRUE &&
            (has_device_extension(physical, "VK_KHR_shader_float16_int8") ||
             properties.apiVersion >= VK_API_VERSION_1_2);
        CHECK_INT(caps.has_int_dot_product, dot_expect);
        CHECK_INT(caps.has_fp16, f16_expect);
    }
    CHECK_INT(caps.is_moltenvk, has_device_extension(physical, "VK_KHR_portability_subset"));
    printf("caps: dot=%u fp16=%u moltenvk=%u\n", (unsigned)caps.has_int_dot_product,
           (unsigned)caps.has_fp16, (unsigned)caps.is_moltenvk);
    /* Vendor runtimes are not filled until 5.04 and 5.06. */
    CHECK_INT(caps.xess_runtime_available, 0);
    CHECK_INT(caps.dlss_runtime_available, 0);

    /* FSR 4 assets: no folder, an empty folder, a missing folder, a present folder. */
    CHECK_INT(query(physical, instance, NULL, &caps), USCALE_OK);
    CHECK_INT(caps.fsr4_assets_available, 0);
    CHECK_INT(query(physical, instance, argv[1], &caps), USCALE_OK);
    CHECK_INT(caps.fsr4_assets_available, 0);
    CHECK_INT(query(physical, instance, "uscale_no_such_fsr4_dir", &caps), USCALE_OK);
    CHECK_INT(caps.fsr4_assets_available, 0);
    CHECK_INT(query(physical, instance, argv[2], &caps), USCALE_OK);
    CHECK_INT(caps.fsr4_assets_available, 1);

    vkDestroyInstance(instance, NULL);
    USCALE_TEST_END;
}
