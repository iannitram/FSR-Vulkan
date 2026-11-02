/* uscale_query_caps: stub (code map: universal/src, planned). SPDX-License-Identifier: MIT */
#include "uscale_vulkan.h"

uscale_result uscale_query_caps(VkPhysicalDevice physical_device,
                                PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                                VkInstance instance, const char *fsr4_asset_dir,
                                uscale_device_caps *out_caps)
{
    (void)physical_device;
    (void)get_instance_proc_addr;
    (void)instance;
    (void)fsr4_asset_dir;
    (void)out_caps;
    return USCALE_ERROR_NOT_IMPLEMENTED;
}
