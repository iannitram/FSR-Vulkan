/*
 * Copyright (c) 2026 Q2RTX FSR Vulkan contributors
 * SPDX-License-Identifier: MIT
 *
 * Pure check of ffxGetVKBufferUsageFlagsFromResourceUsage: no GPU, no device.
 * The FSR 3 Vulkan backend binds read-only structured buffers as
 * VK_DESCRIPTOR_TYPE_STORAGE_BUFFER and copies them, so the usage flags must
 * include STORAGE_BUFFER and TRANSFER_SRC/DST for read-only buffers.
 */

#include <FidelityFX/host/ffx_types.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstdio>

// Defined in sdk/src/backends/vk/ffx_vk.cpp; not declared in a public header.
VkBufferUsageFlags ffxGetVKBufferUsageFlagsFromResourceUsage(FfxResourceUsage flags);

namespace {

int g_failures = 0;

void check(bool condition, const char* what, VkBufferUsageFlags value)
{
    if (condition) {
        std::printf("PASS %s (0x%x)\n", what, static_cast<unsigned>(value));
    } else {
        std::printf("FAIL %s (got 0x%x)\n", what, static_cast<unsigned>(value));
        ++g_failures;
    }
}

const VkBufferUsageFlags kTransfer = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

} // namespace

int main()
{
    // Read-only SRV: bound as STORAGE_BUFFER, and as UNIFORM_BUFFER for cbuffers.
    const VkBufferUsageFlags readOnly = ffxGetVKBufferUsageFlagsFromResourceUsage(FFX_RESOURCE_USAGE_READ_ONLY);
    check((readOnly & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0, "read-only has STORAGE_BUFFER", readOnly);
    check((readOnly & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0, "read-only has UNIFORM_BUFFER", readOnly);
    check((readOnly & kTransfer) == kTransfer, "read-only has TRANSFER_SRC|TRANSFER_DST", readOnly);
    check((readOnly & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) == 0, "read-only has no INDIRECT_BUFFER", readOnly);

    // UAV: STORAGE_BUFFER and transfer only.
    const VkBufferUsageFlags uav = ffxGetVKBufferUsageFlagsFromResourceUsage(FFX_RESOURCE_USAGE_UAV);
    check((uav & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0, "UAV has STORAGE_BUFFER", uav);
    check((uav & kTransfer) == kTransfer, "UAV has TRANSFER_SRC|TRANSFER_DST", uav);
    check((uav & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) == 0, "UAV has no UNIFORM_BUFFER", uav);

    // INDIRECT is kept on top of either usage.
    const VkBufferUsageFlags indirectUav =
        ffxGetVKBufferUsageFlagsFromResourceUsage(
            static_cast<FfxResourceUsage>(FFX_RESOURCE_USAGE_UAV | FFX_RESOURCE_USAGE_INDIRECT));
    check((indirectUav & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) != 0, "UAV|INDIRECT has INDIRECT_BUFFER", indirectUav);
    check((indirectUav & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0, "UAV|INDIRECT has STORAGE_BUFFER", indirectUav);

    const VkBufferUsageFlags indirectReadOnly =
        ffxGetVKBufferUsageFlagsFromResourceUsage(
            static_cast<FfxResourceUsage>(FFX_RESOURCE_USAGE_READ_ONLY | FFX_RESOURCE_USAGE_INDIRECT));
    check((indirectReadOnly & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) != 0, "read-only|INDIRECT has INDIRECT_BUFFER", indirectReadOnly);

    if (g_failures != 0) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all buffer usage checks passed\n");
    return 0;
}
