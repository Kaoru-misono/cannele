#pragma once

#include "RHI.hpp"

#include <volk.h>

namespace cannele::inline graphics::rhi
{
    struct VulkanDeviceCreateInfo final
    {
        bool enable_validation{true};
        bool enable_debug_utils{true};
        bool enable_hdr{true};
        bool enable_ray_tracing{false};

        size_t upload_block_size = 64 * 1024 * 1024;

        uint32_t max_time_queries = 256;

        std::vector<char const*> instance_extensions{};
        std::vector<char const*> device_extensions{};

        VkAllocationCallbacks* allocation_callbacks{};
    };

    struct IVulkanDevice: IDevice
    {
        CNE_INTERFACE(IVulkanDevice);
    };

    auto create_device(VulkanDeviceCreateInfo const* info) -> std::shared_ptr<IVulkanDevice>;
}
