#pragma once

#include <core/assert.hpp>

#include <volk.h>
#include <memory>

namespace cannele::inline graphics::rhi::vk
{
    struct VulkanDevice;
    struct VulkanQueue;
    struct VulkanBuffer;
    struct VulkanTexture;
    struct VulkanTextureView;
    struct VulkanCommandBufferDeprecated;
    struct VulkanCommandManager;
    struct VulkanBindlessManager;
    struct VulkanBufferManager;
    struct VulkanTextureManager;
    struct VulkanGraphicsPipeline;
    struct VulkanComputePipeline;
    struct VulkanTimerQueryPool;

    template <typename T, typename U>
    auto pointer_cast(std::shared_ptr<U> u) -> std::shared_ptr<T>
    {
        static_assert(!std::is_same_v<T, U>, "Redundant cast between same types");

        #if CNE_DEBUG
            if (!u) return nullptr;
            auto t = std::dynamic_pointer_cast<T>(u);
            CNE_ASSERT_WITH(t, "Invalid cast.");
            return t;
        #else
            return std::static_pointer_cast<T> u;
        #endif
    }

    template <typename T, typename U> requires (std::is_pointer_v<U> && std::is_pointer_v<T>)
    auto cast(U u) -> T
    {
        static_assert(!std::is_same_v<T, U>, "Redundant cast between same types");

        #if CNE_DEBUG
            if (!u) return nullptr;
            auto t = dynamic_cast<T>(u);
            CNE_ASSERT_WITH(t, "Invalid cast.");
            return t;
        #else
            return (T*) u;
        #endif
    }
}
