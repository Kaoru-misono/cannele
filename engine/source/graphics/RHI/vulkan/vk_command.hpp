#pragma once

#include "vk_forward.hpp"
#include "vk_shader.hpp"
#include "../resource.hpp"

namespace cannele::inline graphics::rhi::vk
{
    struct VulkanQueue;

    struct VulkanCommandBuffer: RHICommandBuffer
    {
        VulkanQueue* queue{};
        VkCommandBuffer command_buffer{VK_NULL_HANDLE};
        VkCommandPool command_pool{VK_NULL_HANDLE};

        BindingCache binding_cache{};

        uint64_t submission_time{0};

        VulkanCommandBuffer(VulkanDevice* device, VulkanQueue* queue);
        ~VulkanCommandBuffer() override;

        auto reset() -> void override;
    };

    struct VulkanCommandEncoder: CommandEncoder
    {
        VulkanQueue* queue{};
        std::shared_ptr<VulkanCommandBuffer> command_buffer{};

        VulkanCommandEncoder(VulkanDevice* device, VulkanQueue* queue);
        ~VulkanCommandEncoder() override;

        auto finish() -> std::shared_ptr<RHICommandBuffer> override;
        auto binding_data(RootShaderObject* root_object) -> BindingData* override;
    };
}
