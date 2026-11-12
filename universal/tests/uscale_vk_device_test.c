/* Create a real Vulkan device; Linux CI requires Mesa lavapipe.
 * SPDX-License-Identifier: MIT */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdlib.h>

#include "uscale_vulkan.h"
#include "uscale_test.h"

static int unavailable(const char *reason, VkResult result)
{
    const char *required = getenv("USCALE_REQUIRE_VK_DEVICE");
    int fail = required != NULL && strcmp(required, "1") == 0;
    fprintf(stderr, "%s: %s (VkResult %d)\n",
            fail ? "FAIL" : "SKIP", reason, (int)result);
    return fail ? 1 : 77;
}

int main(void)
{
    VkInstanceCreateInfo instance_info = {0};
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties;
    VkQueueFamilyProperties *families;
    VkDeviceQueueCreateInfo queue_info = {0};
    VkDeviceCreateInfo device_info = {0};
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t count = 1, family_count = 0, family;
    float priority = 1.0f;
    VkResult result;

    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    result = vkCreateInstance(&instance_info, NULL, &instance);
    if (result != VK_SUCCESS)
        return unavailable("cannot create Vulkan instance", result);

    /* One physical device is enough; VK_INCOMPLETE means more are available. */
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

    vkGetPhysicalDeviceQueueFamilyProperties(physical, &family_count, NULL);
    families = calloc(family_count, sizeof(*families));
    if (families == NULL || family_count == 0) {
        free(families);
        vkDestroyInstance(instance, NULL);
        fprintf(stderr, "cannot obtain Vulkan queue families\n");
        return 1;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &family_count, families);
    for (family = 0; family < family_count; ++family) {
        if (families[family].queueCount > 0)
            break;
    }
    free(families);
    if (family == family_count) {
        vkDestroyInstance(instance, NULL);
        fprintf(stderr, "no Vulkan queue available\n");
        return 1;
    }

    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    result = vkCreateDevice(physical, &device_info, NULL, &device);
    CHECK_INT(result, VK_SUCCESS);
    if (result == VK_SUCCESS) {
        vkGetDeviceQueue(device, family, 0, &queue);
        CHECK(queue != VK_NULL_HANDLE);
        vkGetPhysicalDeviceProperties(physical, &properties);
        printf("Created Vulkan device: %s (type %d)\n",
               properties.deviceName, (int)properties.deviceType);
        vkDestroyDevice(device, NULL);
    }
    vkDestroyInstance(instance, NULL);
    USCALE_TEST_END;
}
