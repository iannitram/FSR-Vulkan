/* FSR 3.1.6 frame generation on a real device (SPEC M5, #13): two real
 * frames through uscale_record_framegen_prepare / _dispatch, 64x64 render to
 * 128x128 display. Frame 1 (reset) plans the real scene in both slots; frame
 * 2 writes an interpolated image into `generated` and plans generated then
 * real. Prefers Mesa lavapipe (CI), otherwise the first device. Built only
 * with USCALE_WITH_FFX. CTest skips (77) without a device unless
 * USCALE_REQUIRE_VK_DEVICE=1; the Khronos validation layer is used when
 * installed and required when USCALE_REQUIRE_VALIDATION=1.
 * SPDX-License-Identifier: MIT */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdint.h>
#include <stdlib.h>

#include "uscale_vulkan.h"
#include "uscale_test.h"
#include "ffx_vk_vulkan_compat.h" /* VK_KHR_compute_shader_derivatives on older headers */

#define RW 64u
#define RH 64u
#define DW 128u
#define DH 128u

static int g_validation_errors = 0;
/* Upstream's FSR 3.1.6 frame-generation shaders declare compute shader
 * derivatives. A device without VK_KHR_compute_shader_derivatives cannot
 * enable it; only those shader-module messages are then tolerated. */
static int g_have_derivatives = 0;
static int g_tolerated = 0;

static int env_is_1(const char *name)
{
    const char *v = getenv(name);
    return v != NULL && strcmp(v, "1") == 0;
}

static int unavailable(const char *reason, VkResult result)
{
    int fail = env_is_1("USCALE_REQUIRE_VK_DEVICE");
    fprintf(stderr, "%s: %s (VkResult %d)\n", fail ? "FAIL" : "SKIP", reason, (int)result);
    return fail ? 1 : 77;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL on_message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                 VkDebugUtilsMessageTypeFlagsEXT types,
                                                 const VkDebugUtilsMessengerCallbackDataEXT *data,
                                                 void *user)
{
    (void)types;
    (void)user;
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) && !g_have_derivatives &&
        (strstr(data->pMessage, "compute_shader_derivatives") != NULL ||
         strstr(data->pMessage, "ComputeDerivativeGroup") != NULL)) {
        g_tolerated++;
        return VK_FALSE;
    }
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        fprintf(stderr, "validation error: %s\n", data->pMessage);
        g_validation_errors++;
    }
    return VK_FALSE;
}

static int has_instance_name(const char *name, int layer)
{
    uint32_t n = 0, i;
    int found = 0;
    if (layer) {
        VkLayerProperties *p;
        vkEnumerateInstanceLayerProperties(&n, NULL);
        p = calloc(n ? n : 1, sizeof(*p));
        if (p == NULL) return 0;
        vkEnumerateInstanceLayerProperties(&n, p);
        for (i = 0; i < n; ++i) found |= strcmp(p[i].layerName, name) == 0;
        free(p);
    } else {
        VkExtensionProperties *p;
        vkEnumerateInstanceExtensionProperties(NULL, &n, NULL);
        p = calloc(n ? n : 1, sizeof(*p));
        if (p == NULL) return 0;
        vkEnumerateInstanceExtensionProperties(NULL, &n, p);
        for (i = 0; i < n; ++i) found |= strcmp(p[i].extensionName, name) == 0;
        free(p);
    }
    return found;
}

static int has_device_extension(VkPhysicalDevice phys, const char *name)
{
    uint32_t n = 0, i;
    int found = 0;
    VkExtensionProperties *p;
    vkEnumerateDeviceExtensionProperties(phys, NULL, &n, NULL);
    p = calloc(n ? n : 1, sizeof(*p));
    if (p == NULL) return 0;
    vkEnumerateDeviceExtensionProperties(phys, NULL, &n, p);
    for (i = 0; i < n; ++i) found |= strcmp(p[i].extensionName, name) == 0;
    free(p);
    return found;
}

static uint32_t find_memory(VkPhysicalDevice phys, uint32_t bits, VkMemoryPropertyFlags want)
{
    VkPhysicalDeviceMemoryProperties mp;
    uint32_t i;
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (i = 0; i < mp.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want)
            return i;
    return UINT32_MAX;
}

typedef struct image {
    VkImage image;
    VkDeviceMemory memory;
} image;

static VkResult make_image(VkDevice dev, VkPhysicalDevice phys, VkFormat format, uint32_t w,
                           uint32_t h, VkImageUsageFlags usage, image *out)
{
    VkImageCreateInfo ci = {0};
    VkMemoryRequirements req;
    VkMemoryAllocateInfo ai = {0};
    VkResult r;
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent.width = w;
    ci.extent.height = h;
    ci.extent.depth = 1;
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    r = vkCreateImage(dev, &ci, NULL, &out->image);
    if (r != VK_SUCCESS) return r;
    vkGetImageMemoryRequirements(dev, out->image, &req);
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX)
        ai.memoryTypeIndex = find_memory(phys, req.memoryTypeBits, 0);
    r = vkAllocateMemory(dev, &ai, NULL, &out->memory);
    if (r != VK_SUCCESS) return r;
    return vkBindImageMemory(dev, out->image, out->memory, 0);
}

static void to_general(VkCommandBuffer cb, VkImage img, VkImageAspectFlags aspect)
{
    VkImageMemoryBarrier b = {0};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = aspect;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, NULL, 0, NULL, 1, &b);
}

static void full_barrier(VkCommandBuffer cb)
{
    VkMemoryBarrier m = {0};
    m.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    m.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    m.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 1, &m, 0, NULL, 0, NULL);
}

static uscale_image describe(VkImage img, VkFormat format, uint32_t w, uint32_t h)
{
    uscale_image d = {0};
    d.image = img;
    d.view = VK_NULL_HANDLE;
    d.format = format;
    d.extent.width = w;
    d.extent.height = h;
    d.layout = VK_IMAGE_LAYOUT_GENERAL;
    return d;
}

int main(void)
{
    const char *layer = "VK_LAYER_KHRONOS_validation";
    const char *utils_ext = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    VkInstanceCreateInfo ici = {0};
    VkApplicationInfo app = {0};
    VkDebugUtilsMessengerCreateInfoEXT mci = {0};
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    PFN_vkCreateDebugUtilsMessengerEXT create_messenger = NULL;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger = NULL;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice *devices, phys = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features = {0}, supported;
    VkDeviceQueueCreateInfo qci = {0};
    VkDeviceCreateInfo dci = {0};
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue;
    VkQueueFamilyProperties *families;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cb;
    VkCommandPoolCreateInfo pci = {0};
    VkCommandBufferAllocateInfo cai = {0};
    VkCommandBufferBeginInfo bi = {0};
    VkSubmitInfo si = {0};
    VkBufferCreateInfo bci = {0};
    VkBuffer readback = VK_NULL_HANDLE;
    VkDeviceMemory readback_mem = VK_NULL_HANDLE;
    VkMemoryRequirements breq;
    VkMemoryAllocateInfo bai = {0};
    VkBufferImageCopy copy = {0};
    VkClearColorValue color_clear = {{0.5f, 0.25f, 0.75f, 1.0f}}, zero = {{0, 0, 0, 0}};
    VkClearColorValue depth_clear = {{0.5f, 0, 0, 0}};
    VkImageSubresourceRange color_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    image color = {0}, depth = {0}, motion = {0}, generated = {0};
    VkSemaphore sem[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkSemaphoreCreateInfo sci = {0};
    VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR derivatives = {0};
    VkPhysicalDeviceFeatures2 features2 = {0};
    const char *derivatives_ext = VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME;
    uscale_context_create_info info = {0};
    uscale_framegen_prepare_info prep = {0};
    uscale_framegen_dispatch_info disp = {0};
    uscale_present_plan plan;
    uscale_context *ctx = NULL;
    uscale_result ur;
    uint32_t count = 0, i, family = 0, family_count = 0, frame;
    uint8_t *pixels = NULL;
    size_t px, filled = 0, center_px;
    int use_validation, have_utils;
    float priority = 1.0f;
    VkResult r;

    use_validation = has_instance_name(layer, 1);
    have_utils = has_instance_name(utils_ext, 0);
    if (!use_validation && env_is_1("USCALE_REQUIRE_VALIDATION")) {
        fprintf(stderr, "FAIL: %s is not installed\n", layer);
        return 1;
    }
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    /* FSR 3 shaders are SPIR-V 1.5 (upstream builds them for vulkan1.2). */
    app.apiVersion = VK_API_VERSION_1_2;
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    if (use_validation) {
        ici.enabledLayerCount = 1;
        ici.ppEnabledLayerNames = &layer;
    }
    if (have_utils) {
        ici.enabledExtensionCount = 1;
        ici.ppEnabledExtensionNames = &utils_ext;
    }
    r = vkCreateInstance(&ici, NULL, &instance);
    if (r != VK_SUCCESS) return unavailable("cannot create Vulkan instance", r);
    if (use_validation && have_utils) {
        mci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        mci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        mci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        mci.pfnUserCallback = on_message;
        create_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        destroy_messenger = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (create_messenger != NULL)
            r = create_messenger(instance, &mci, NULL, &messenger);
        else
            r = VK_ERROR_EXTENSION_NOT_PRESENT;
        if (r != VK_SUCCESS)
            messenger = VK_NULL_HANDLE;
    }
    if (env_is_1("USCALE_REQUIRE_VALIDATION") && messenger == VK_NULL_HANDLE) {
        fprintf(stderr, "FAIL: validation required but no debug messenger "
                        "(%s %s)\n", utils_ext, have_utils ? "present, create failed" : "missing");
        vkDestroyInstance(instance, NULL);
        return 1;
    }
    printf("Validation layer: %s\n", use_validation ? "on" : "not installed");

    vkEnumeratePhysicalDevices(instance, &count, NULL);
    if (count == 0) {
        vkDestroyInstance(instance, NULL);
        return unavailable("no Vulkan physical device", VK_SUCCESS);
    }
    devices = calloc(count, sizeof(*devices));
    if (devices == NULL) return 1;
    vkEnumeratePhysicalDevices(instance, &count, devices);
    phys = devices[0];
    for (i = 0; i < count; ++i) { /* prefer lavapipe (a CPU device), as CI does */
        vkGetPhysicalDeviceProperties(devices[i], &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
            phys = devices[i];
            break;
        }
    }
    free(devices);
    vkGetPhysicalDeviceProperties(phys, &props);
    printf("Device: %s (type %d)\n", props.deviceName, (int)props.deviceType);
    if (props.apiVersion < VK_API_VERSION_1_2) {
        vkDestroyInstance(instance, NULL);
        return unavailable("device below Vulkan 1.2", VK_SUCCESS);
    }

    vkGetPhysicalDeviceFeatures(phys, &supported);
    if (!supported.shaderStorageImageWriteWithoutFormat) {
        vkDestroyInstance(instance, NULL);
        return unavailable("no shaderStorageImageWriteWithoutFormat", VK_SUCCESS);
    }
    /* The game must enable this on its device (uscale_vulkan.h). */
    features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
    features.shaderStorageImageReadWithoutFormat = supported.shaderStorageImageReadWithoutFormat;

    vkGetPhysicalDeviceQueueFamilyProperties(phys, &family_count, NULL);
    families = calloc(family_count ? family_count : 1, sizeof(*families));
    if (families == NULL) return 1;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &family_count, families);
    for (family = 0; family < family_count; ++family)
        if (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT) break;
    free(families);
    if (family == family_count) {
        fprintf(stderr, "no compute queue\n");
        return 1;
    }
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.pEnabledFeatures = &features;
    /* Frame generation: enable compute shader derivatives when offered. */
    derivatives.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR;
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &derivatives;
    if (has_device_extension(phys, derivatives_ext)) {
        vkGetPhysicalDeviceFeatures2(phys, &features2);
        if (derivatives.computeDerivativeGroupLinear || derivatives.computeDerivativeGroupQuads) {
            derivatives.pNext = NULL;
            dci.pNext = &derivatives;
            dci.enabledExtensionCount = 1;
            dci.ppEnabledExtensionNames = &derivatives_ext;
            g_have_derivatives = 1;
        }
    }
    printf("%s: %s\n", derivatives_ext, g_have_derivatives ? "enabled" : "not offered");
    r = vkCreateDevice(phys, &dci, NULL, &device);
    CHECK_INT(r, VK_SUCCESS);
    if (r != VK_SUCCESS) return 1;
    vkGetDeviceQueue(device, family, 0, &queue);

    /* Frame resources: display-size real frame and generated target in the
     * frame-generation colour format; render-size R32_SFLOAT depth and
     * R16G16_SFLOAT motion. Everything stays in GENERAL. */
    /* Upstream copies the real frame for its history: sampled + transfer source. */
    CHECK_INT(make_image(device, phys, VK_FORMAT_R8G8B8A8_UNORM, DW, DH,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT, &color),
              VK_SUCCESS);
    CHECK_INT(make_image(device, phys, VK_FORMAT_R32_SFLOAT, RW, RH,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &depth),
              VK_SUCCESS);
    CHECK_INT(make_image(device, phys, VK_FORMAT_R16G16_SFLOAT, RW, RH,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &motion),
              VK_SUCCESS);
    CHECK_INT(make_image(device, phys, VK_FORMAT_R8G8B8A8_UNORM, DW, DH,
                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                         &generated),
              VK_SUCCESS);
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = (VkDeviceSize)DW * DH * 4u;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    CHECK_INT(vkCreateBuffer(device, &bci, NULL, &readback), VK_SUCCESS);
    vkGetBufferMemoryRequirements(device, readback, &breq);
    bai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bai.allocationSize = breq.size;
    bai.memoryTypeIndex = find_memory(phys, breq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CHECK(bai.memoryTypeIndex != UINT32_MAX);
    CHECK_INT(vkAllocateMemory(device, &bai, NULL, &readback_mem), VK_SUCCESS);
    CHECK_INT(vkBindBufferMemory(device, readback, readback_mem, 0), VK_SUCCESS);
    /* Stand-ins for the game's acquire semaphores; the plan hands them back. */
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    CHECK_INT(vkCreateSemaphore(device, &sci, NULL, &sem[0]), VK_SUCCESS);
    CHECK_INT(vkCreateSemaphore(device, &sci, NULL, &sem[1]), VK_SUCCESS);
    if (uscale_test_failures != 0) return 1;

    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = family;
    CHECK_INT(vkCreateCommandPool(device, &pci, NULL, &pool), VK_SUCCESS);
    cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cai.commandPool = pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    CHECK_INT(vkAllocateCommandBuffers(device, &cai, &cb), VK_SUCCESS);

    /* The context, through the public API, with frame generation. */
    info.struct_size = sizeof(info);
    info.api_version = USCALE_VERSION;
    info.instance = instance;
    info.physical_device = phys;
    info.device = device;
    info.get_instance_proc_addr = vkGetInstanceProcAddr;
    info.get_device_proc_addr = vkGetDeviceProcAddr;
    info.preferred_backend = USCALE_BACKEND_FSR3;
    info.quality = USCALE_QUALITY_PERFORMANCE;
    info.max_render_extent.width = RW;
    info.max_render_extent.height = RH;
    info.display_extent.width = DW;
    info.display_extent.height = DH;
    info.enable_frame_generation = 1;
    info.framegen_color_format = VK_FORMAT_R8G8B8A8_UNORM;
    ur = uscale_context_create(&info, &ctx);
    CHECK_INT(ur, USCALE_OK);
    if (ur != USCALE_OK || ctx == NULL) {
        fprintf(stderr, "uscale_context_create failed: %s\n", uscale_result_name(ur));
        return 1;
    }

    prep.struct_size = sizeof(prep);
    prep.present_color = describe(color.image, VK_FORMAT_R8G8B8A8_UNORM, DW, DH);
    prep.depth = describe(depth.image, VK_FORMAT_R32_SFLOAT, RW, RH);
    prep.motion = describe(motion.image, VK_FORMAT_R16G16_SFLOAT, RW, RH);
    prep.render_extent.width = RW;
    prep.render_extent.height = RH;
    prep.frame_time_ms = 16.6667f;
    prep.camera_near = 0.1f;
    prep.camera_far = 1000.0f;
    prep.camera_fov_y = 1.0471975512f;
    prep.view_space_to_meters = 1.0f;
    prep.camera_up[1] = 1.0f;
    prep.camera_right[0] = 1.0f;
    prep.camera_forward[2] = -1.0f;
    prep.max_luminance = 1.0f;
    prep.transfer = USCALE_TRANSFER_SRGB;
    disp.struct_size = sizeof(disp);
    disp.present_color = prep.present_color;
    disp.generated = describe(generated.image, VK_FORMAT_R8G8B8A8_UNORM, DW, DH);
    disp.frame_time_ms = prep.frame_time_ms;
    /* The game acquired swapchain image 1 for the generated frame, then 0. */
    disp.acquired.generated_index = 1;
    disp.acquired.real_index = 0;
    disp.acquired.generated_available = sem[0];
    disp.acquired.real_available = sem[1];
    disp.acquired.generated_acquired = 1;
    disp.acquired.real_acquired = 1;
    disp.acquired.paired = 1;

    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    for (frame = 1; frame <= 2; ++frame) {
        CHECK_INT(vkResetCommandBuffer(cb, 0), VK_SUCCESS);
        CHECK_INT(vkBeginCommandBuffer(cb, &bi), VK_SUCCESS);
        if (frame == 1) {
            to_general(cb, color.image, VK_IMAGE_ASPECT_COLOR_BIT);
            to_general(cb, depth.image, VK_IMAGE_ASPECT_COLOR_BIT);
            to_general(cb, motion.image, VK_IMAGE_ASPECT_COLOR_BIT);
            to_general(cb, generated.image, VK_IMAGE_ASPECT_COLOR_BIT);
        }
        /* "Render" the real frame; clear the target so frame 2 must fill it. */
        vkCmdClearColorImage(cb, color.image, VK_IMAGE_LAYOUT_GENERAL, &color_clear, 1,
                             &color_range);
        vkCmdClearColorImage(cb, depth.image, VK_IMAGE_LAYOUT_GENERAL, &depth_clear, 1,
                             &color_range);
        vkCmdClearColorImage(cb, motion.image, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &color_range);
        vkCmdClearColorImage(cb, generated.image, VK_IMAGE_LAYOUT_GENERAL, &zero, 1,
                             &color_range);
        full_barrier(cb);

        prep.command_buffer = cb;
        prep.frame_id = frame;
        prep.reset = frame == 1;
        disp.command_buffer = cb;
        disp.frame_id = frame;
        disp.reset = frame == 1;
        CHECK_INT(uscale_record_framegen_prepare(ctx, &prep), USCALE_OK);
        ur = uscale_record_framegen_dispatch(ctx, &disp, &plan);
        CHECK_INT(ur, USCALE_OK);
        /* Two slots, generated target then real target, never reordered. */
        CHECK_INT(plan.slot_count, 2);
        CHECK_INT(plan.slots[0].image_index, 1);
        CHECK(plan.slots[0].image_available == sem[0]);
        CHECK_INT(plan.slots[1].image_index, 0);
        CHECK(plan.slots[1].image_available == sem[1]);
        CHECK_INT(plan.slots[1].use_generated, 0);
        /* Frame 1 is a reset: slot 0 shows the real scene but is presented. */
        CHECK_INT(plan.slots[0].use_generated, frame == 1 ? 0 : 1);
        printf("frame %u: plan %u slots: [%u %s] [%u %s]\n", frame, plan.slot_count,
               plan.slots[0].image_index, plan.slots[0].use_generated ? "generated" : "real",
               plan.slots[1].image_index, plan.slots[1].use_generated ? "generated" : "real");
        full_barrier(cb);
        if (frame == 2) {
            copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.layerCount = 1;
            copy.imageExtent.width = DW;
            copy.imageExtent.height = DH;
            copy.imageExtent.depth = 1;
            vkCmdCopyImageToBuffer(cb, generated.image, VK_IMAGE_LAYOUT_GENERAL, readback, 1,
                                   &copy);
        }
        CHECK_INT(vkEndCommandBuffer(cb), VK_SUCCESS);
        CHECK_INT(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE), VK_SUCCESS);
        CHECK_INT(vkQueueWaitIdle(queue), VK_SUCCESS);
        CHECK_INT(uscale_retire_frame(ctx, frame), USCALE_OK);
    }

    /* The interpolated frame between two identical real frames is that frame. */
    CHECK_INT(vkMapMemory(device, readback_mem, 0, VK_WHOLE_SIZE, 0, (void **)&pixels), VK_SUCCESS);
    if (pixels != NULL) {
        for (px = 0; px < (size_t)DW * DH; ++px)
            filled += pixels[px * 4u] != 0 || pixels[px * 4u + 1u] != 0 ||
                      pixels[px * 4u + 2u] != 0;
        center_px = ((size_t)(DH / 2u) * DW + DW / 2u) * 4u;
        printf("Generated pixels with colour: %zu of %zu; centre RGBA %u %u %u %u\n", filled,
               (size_t)DW * DH, pixels[center_px], pixels[center_px + 1u],
               pixels[center_px + 2u], pixels[center_px + 3u]);
        CHECK(pixels[center_px] != 0 || pixels[center_px + 1u] != 0 ||
              pixels[center_px + 2u] != 0);
        vkUnmapMemory(device, readback_mem);
    }
    CHECK(filled > 0);

    uscale_context_destroy(ctx);
    vkDestroySemaphore(device, sem[0], NULL);
    vkDestroySemaphore(device, sem[1], NULL);
    vkDestroyCommandPool(device, pool, NULL);
    vkDestroyBuffer(device, readback, NULL);
    vkFreeMemory(device, readback_mem, NULL);
    vkDestroyImage(device, color.image, NULL);
    vkFreeMemory(device, color.memory, NULL);
    vkDestroyImage(device, depth.image, NULL);
    vkFreeMemory(device, depth.memory, NULL);
    vkDestroyImage(device, motion.image, NULL);
    vkFreeMemory(device, motion.memory, NULL);
    vkDestroyImage(device, generated.image, NULL);
    vkFreeMemory(device, generated.memory, NULL);
    vkDestroyDevice(device, NULL);
    if (messenger != VK_NULL_HANDLE && destroy_messenger != NULL)
        destroy_messenger(instance, messenger, NULL);
    vkDestroyInstance(instance, NULL);
    if (g_tolerated != 0)
        printf("WARNING: %d compute-shader-derivative messages tolerated: the device does "
               "not offer %s\n", g_tolerated, derivatives_ext);
    CHECK_INT(g_validation_errors, 0);
    USCALE_TEST_END;
}
