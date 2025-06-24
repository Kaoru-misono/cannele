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

        platform::Window* window{};
    };

    struct IVulkanDevice: IDevice
    {
        CNE_INTERFACE(IVulkanDevice);
    };

    auto create_device(VulkanDeviceCreateInfo* info) -> DeviceHandle;
}
