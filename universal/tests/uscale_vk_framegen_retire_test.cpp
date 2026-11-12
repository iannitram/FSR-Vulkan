/* SPDX-License-Identifier: MIT
 * #28: exercise the real FI/OF scheduler and count actual Vulkan allocations.
 * Reuse upstream's image/device helpers and valid prepare/dispatch fixtures.
 * Its validation-only main is compiled under another name, never called here.
 */
#define main upstream_framegen_smoke_main
#include "../../tests/fsr3_3_1_6_framegen_api_smoke.cpp"
#undef main

#include <algorithm>
#include <map>

namespace {
std::map<VkDeviceMemory, VkDeviceSize> liveMemory;
std::map<VkDescriptorPool, uint32_t> livePools;
PFN_vkAllocateMemory allocateMemory = nullptr;
PFN_vkFreeMemory freeMemory = nullptr;
PFN_vkCreateDescriptorPool createDescriptorPool = nullptr;
PFN_vkDestroyDescriptorPool destroyDescriptorPool = nullptr;

VkDeviceSize live_bytes()
{
    VkDeviceSize bytes = 0;
    for (const auto& allocation : liveMemory)
        bytes += allocation.second;
    return bytes;
}

// All bridge descriptors use one pool per set. Interpose at the Vulkan ABI so
// neither private bridge layouts nor its resource-only memory query are needed.
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(
    VkDevice device, const VkMemoryAllocateInfo* info,
    const VkAllocationCallbacks* callbacks, VkDeviceMemory* memory)
{
    const VkResult result = allocateMemory(device, info, callbacks, memory);
    if (result == VK_SUCCESS)
        liveMemory.emplace(*memory, info->allocationSize);
    return result;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkFreeMemory(
    VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* callbacks)
{
    liveMemory.erase(memory);
    freeMemory(device, memory, callbacks);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorPool(
    VkDevice device, const VkDescriptorPoolCreateInfo* info,
    const VkAllocationCallbacks* callbacks, VkDescriptorPool* pool)
{
    const VkResult result = createDescriptorPool(device, info, callbacks, pool);
    if (result == VK_SUCCESS)
        livePools.emplace(*pool, info->maxSets);
    return result;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(
    VkDevice device, VkDescriptorPool pool, const VkAllocationCallbacks* callbacks)
{
    livePools.erase(pool);
    destroyDescriptorPool(device, pool, callbacks);
}

int main()
{
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    FfxVkFsr3_3_1_6FrameGenerationContext* context = nullptr;
    Image images[5]{};
    uint32_t family = 0;
    int result = 1;
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceInfo.pApplicationInfo = &app;
    if (!expect(vkCreateInstance(&instanceInfo, nullptr, &instance) == VK_SUCCESS &&
                choose_compute_device(instance, &physical, &family), "find Vulkan GPU"))
        goto cleanup;
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical, &properties);
        std::printf("GPU: %s; driver=%u\n", properties.deviceName, properties.driverVersion);
        const float priority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        VkPhysicalDeviceFeatures features{};
        features.shaderStorageImageReadWithoutFormat = VK_TRUE;
        features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.pEnabledFeatures = &features;
        if (!expect(vkCreateDevice(physical, &deviceInfo, nullptr, &device) == VK_SUCCESS,
                    "create Vulkan device"))
            goto cleanup;
        allocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(vkGetDeviceProcAddr(device, "vkAllocateMemory"));
        freeMemory = reinterpret_cast<PFN_vkFreeMemory>(vkGetDeviceProcAddr(device, "vkFreeMemory"));
        createDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(vkGetDeviceProcAddr(device, "vkCreateDescriptorPool"));
        destroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(vkGetDeviceProcAddr(device, "vkDestroyDescriptorPool"));
        if (!expect(allocateMemory && freeMemory && createDescriptorPool && destroyDescriptorPool,
                    "resolve real Vulkan allocation functions"))
            goto cleanup;
        vkGetDeviceQueue(device, family, 0, &queue);
    }
    for (uint32_t index = 0; index < 5; ++index) {
        const VkFormat formats[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R32_SFLOAT,
            VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R16G16_SFLOAT};
        const uint32_t size = index == 1 || index == 2 ? 64u : 128u;
        if (!expect(create_image(physical, device, size, size, formats[index], &images[index]),
                    "create frame images"))
            goto cleanup;
    }
    {
        FfxVkFsr3_3_1_6FrameGenerationCreateInfo create{};
        create.physicalDevice = physical;
        create.device = device;
        create.maxRenderWidth = create.maxRenderHeight = 64;
        create.displayWidth = create.displayHeight = 128;
        create.colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
        if (!expect(ffxVkFsr3_3_1_6FrameGenerationContextCreate(&create, &context) ==
                    FFX_VK_FSR3_3_1_6_FRAMEGEN_OK, "create framegen context"))
            goto cleanup;
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.queueFamilyIndex = family;
        if (!expect(vkCreateCommandPool(device, &poolInfo, nullptr, &pool) == VK_SUCCESS,
                    "create command pool"))
            goto cleanup;
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = pool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        if (!expect(vkAllocateCommandBuffers(device, &allocation, &commands) == VK_SUCCESS,
                    "allocate command buffer"))
            goto cleanup;
        VkDeviceSize baselineBytes = 0, boundBytes = 0;
        size_t baselineCount = 0, boundCount = 0, boundPools = 0;
        // One warm-up, then 1,002 generated frames in batches of three. GPU
        // completion precedes retirement; completed frames stay live until then.
        for (uint64_t frame = 1; frame <= 1003; ++frame) {
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            if (!expect(vkResetCommandPool(device, pool, 0) == VK_SUCCESS &&
                        vkBeginCommandBuffer(commands, &begin) == VK_SUCCESS, "begin frame"))
                goto cleanup;
            if (frame == 1) {
                // Otherwise-valid input: zero must be rejected before it can
                // prepare the context or allocate anything under the immortal tag.
                FfxVkFsr3_3_1_6FrameGenerationPrepareInfo zero{};
                zero.commandBuffer = commands;
                zero.color = image_info(images[0], VK_FORMAT_R8G8B8A8_UNORM, 128, 128,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                zero.depth = image_info(images[1], VK_FORMAT_R32_SFLOAT, 64, 64,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                zero.motionVectors = image_info(images[2], VK_FORMAT_R16G16_SFLOAT, 64, 64,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                zero.renderWidth = zero.renderHeight = 64;
                zero.motionVectorScaleX = zero.motionVectorScaleY = 64.0f;
                zero.frameTimeMilliseconds = 16.6667f;
                zero.cameraNear = 0.1f;
                zero.cameraFar = 1000.0f;
                zero.viewSpaceToMeters = 1.0f;
                zero.cameraVerticalFovRadians = 1.0471975512f;
                zero.maxLuminance = 1.0f;
                zero.reset = VK_TRUE;
                const size_t before = liveMemory.size();
                if (!expect(ffxVkFsr3_3_1_6FrameGenerationContextRecordPrepare(context, &zero) ==
                            FFX_VK_FSR3_3_1_6_FRAMEGEN_ERROR_INVALID_ARGUMENT &&
                            liveMemory.size() == before && livePools.empty(),
                            "refuse frame id 0 without allocating"))
                    goto cleanup;
                std::printf("frameId 0: rejected\n");
            }
            if (!expect(record_frame(context, commands, images, VK_FORMAT_R8G8B8A8_UNORM,
                        VK_FORMAT_R16G16_SFLOAT, FFX_VK_FSR3_3_1_6_FRAMEGEN_TRANSFER_SRGB,
                        1.0f, frame, frame == 1, frame == 1, false, true), "record generated frame") ||
                !expect(vkEndCommandBuffer(commands) == VK_SUCCESS, "end frame"))
                goto cleanup;
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &commands;
            if (!expect(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
                        vkQueueWaitIdle(queue) == VK_SUCCESS, "complete frame before retire"))
                goto cleanup;
            if (frame <= 4) {
                boundCount = std::max(boundCount, liveMemory.size());
                boundBytes = std::max(boundBytes, live_bytes());
                boundPools = std::max(boundPools, livePools.size());
            } else if (!expect(liveMemory.size() <= boundCount && live_bytes() <= boundBytes &&
                               livePools.size() <= boundPools, "live allocations bounded by first batch")) {
                goto cleanup;
            }
            if (frame == 1 || (frame - 1) % 3 == 0) {
                if (!expect(!livePools.empty() && liveMemory.size() > baselineCount,
                            "dispatch allocations retained until retirement"))
                    goto cleanup;
                if (frame > 1) {
                    // Retiring older IDs must leave the newest frame's objects
                    // intact even though the queue has already completed it.
                    if (!expect(ffxVkFsr3_3_1_6FrameGenerationContextRetireFrame(context, frame - 1) ==
                                FFX_VK_FSR3_3_1_6_FRAMEGEN_OK && !livePools.empty() &&
                                liveMemory.size() > baselineCount, "retain allocations newer than completed ID"))
                        goto cleanup;
                }
                if (!expect(ffxVkFsr3_3_1_6FrameGenerationContextRetireFrame(context, frame) ==
                            FFX_VK_FSR3_3_1_6_FRAMEGEN_OK, "retire completed batch"))
                    goto cleanup;
                if (frame == 1) {
                    baselineCount = liveMemory.size();
                    baselineBytes = live_bytes();
                }
                if (!expect(liveMemory.size() == baselineCount && live_bytes() == baselineBytes &&
                            livePools.empty(), "retired allocations return to warm baseline"))
                    goto cleanup;
            }
        }
        std::printf("1003 generated frames (1 warm-up + 1002 measured); 3 retained frames max\n"
                    "live memory: baseline=%zu allocations / %llu bytes; peak=%zu / %llu bytes\n"
                    "live descriptor pools/sets: peak=%zu; after each retirement=0\n"
                    "post-retire growth: 0 allocations, 0 bytes\n",
                    baselineCount, static_cast<unsigned long long>(baselineBytes),
                    boundCount, static_cast<unsigned long long>(boundBytes), boundPools);
    }
    result = 0;
cleanup:
    if (device)
        vkDeviceWaitIdle(device);
    ffxVkFsr3_3_1_6FrameGenerationContextDestroy(context);
    for (Image& image : images) {
        if (image.image)
            vkDestroyImage(device, image.image, nullptr);
        if (image.memory)
            vkFreeMemory(device, image.memory, nullptr);
    }
    if (pool)
        vkDestroyCommandPool(device, pool, nullptr);
    if (!expect(liveMemory.empty() && livePools.empty(), "all tracked allocations destroyed"))
        result = 1;
    if (device)
        vkDestroyDevice(device, nullptr);
    if (instance)
        vkDestroyInstance(instance, nullptr);
    return result;
}
