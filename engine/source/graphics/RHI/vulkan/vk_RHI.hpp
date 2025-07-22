#pragma once

#include "../vulkan.hpp"
#include "../tool/resource_pool.hpp"
#include "vk_resource.hpp"

namespace cannele::inline graphics::rhi::vk
{
    struct PhysicalDeviceFeatures final
    {
        PhysicalDeviceFeatures();

        auto connect(auto* next) -> void;

        VkPhysicalDeviceFeatures2 vk_features2{};
        VkPhysicalDeviceVulkan11Features vk11_features{};
        VkPhysicalDeviceVulkan12Features vk12_features{};
        VkPhysicalDeviceVulkan13Features vk13_features{};

        VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{};
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features{};
        VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features{};

        VkPhysicalDeviceExtendedDynamicState2FeaturesEXT extended_dynamic_state2_features{};
        VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extended_dynamic_state3_features{};
        VkPhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features{};
        VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertex_input_dynamic_state_features{};
    };

    struct PhysicalDeviceProperties final
    {
        PhysicalDeviceProperties();

        VkPhysicalDeviceMemoryProperties memoryProperties{};
		VkPhysicalDeviceProperties2 properties2{};
		VkPhysicalDeviceSubgroupProperties subgroup_properties{};
		VkPhysicalDeviceDescriptorIndexingProperties descriptor_indexing_properties{};

		VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties{};
    };

    struct QueueInfo final
    {
        uint32_t graphics_family{~0u};
        uint32_t compute_family{~0u};
        uint32_t transfer_family{~0u};

        struct Queue final
        {
            VkQueue queue{VK_NULL_HANDLE};
            float priority{0.5f};
        };

        std::vector<Queue> graphics_queues{};
        std::vector<Queue> compute_queues{};
        std::vector<Queue> transfer_queues{};
    };

    struct VulkanDevice: IVulkanDevice
    {
        VulkanDeviceCreateInfo device_info{};

        VkInstance instance{};
        VkDevice device{};
        VkPhysicalDevice physical_device{};
        VmaAllocator allocator{};
        VkPipelineCache pipeline_cache{};
        VkAllocationCallbacks* allocation_callbacks{};

        VkDebugUtilsMessengerEXT debug_utils_messenger{};

        // Cached physical device features.
        PhysicalDeviceFeatures physical_device_features{};
        PhysicalDeviceFeatures enabled_physical_device_features{};

        PhysicalDeviceProperties physical_device_properties{};

        // Cached queue info.
        QueueInfo queue_info{};
        ResourceOwned<VulkanQueue> graphics_queue{};
        ResourceOwned<VulkanQueue> async_compute_queue{};
        ResourceOwned<VulkanQueue> async_transfer_queue{};

        ResourceOwned<VulkanLayoutManager> layout_manager{};
        ResourceOwned<VulkanPipelineManager> pipeline_manager{};
        ResourceOwned<ResourcePool<VulkanBuffer>> buffer_pool{};
        ResourceOwned<ResourcePool<VulkanTexture>> texture_pool{};

        std::unordered_map<size_t, RefCountPtr<VulkanSampler>> samplers{};
        ResourceOwned<VulkanBindlessManager> bindless_manager{};

        ResourceOwned<ShaderFactory> shader_factory{};
        // Place swapchain here to make sure order of destruction
        std::mutex mutex{};

        uint32_t frame_count{};
        uint32_t num_rendering_frames{};

        VulkanDevice(VulkanDeviceCreateInfo* info);
        ~VulkanDevice() override;

        auto name() -> std::string override { return "VulkanDevice"; }
        auto backend() -> EBackend override { return EBackend::vulkan; }
        auto new_frame(uint32_t frame_count) -> void override;
        auto create_buffer(std::string_view name, BufferCreateInfo* info) -> BufferHandle override;
        auto create_texture(std::string_view name, TextureCreateInfo* info) -> TextureHandle override;
        auto create_sampler(std::string_view name, SamplerCreateInfo* info) -> SamplerHandle override;
        auto create_swapchain(std::string_view name, SwapchainCreateInfo* info) -> SwapchainHandle;
        auto create_graphics_pipeline(std::string_view name, GraphicsPipelineCreateInfo* info) -> GraphicsPipelineHandle override;
        auto create_compute_pipeline(std::string_view name, ComputePipelineCreateInfo* info) -> ComputePipelineHandle override;
        auto create_shader_module(std::string_view name, ShaderModuleCreateInfo* info) -> ShaderModuleHandle override;
        auto create_command_list(CommandListCreateInfo* info) -> CommandListHandle override;
        auto create_swapchain(SwapchainCreateInfo* info) -> SwapchainHandle override;
        auto get_shader_factory() -> ShaderFactory* override { return shader_factory.get(); }
        auto submit_command_lists(std::span<CommandListHandle> lists, EQueueType type = EQueueType::graphics) -> uint64_t override;
        auto current_timeline_value(EQueueType type) -> uint64_t override;

        auto wait_idle() -> void override;

        auto queue(EQueueType type) -> VulkanQueue*;
        auto queue_family(EQueueType type) -> uint32_t;
    };
}
