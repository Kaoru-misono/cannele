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

        VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer_features{};
        VkPhysicalDeviceExtendedDynamicState2FeaturesEXT extended_dynamic_state2_features{};
        VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extended_dynamic_state3_features{};
        VkPhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features{};
        VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertex_input_dynamic_state_features{};
        VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT mutable_descriptor_type_features{};
        VkPhysicalDevicePipelineBinaryFeaturesKHR pipeline_binary_features{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_BINARY_FEATURES_KHR
        };
    };

    struct PhysicalDeviceProperties final
    {
        PhysicalDeviceProperties();

        VkPhysicalDeviceMemoryProperties memoryProperties{};
		VkPhysicalDeviceProperties2 properties2{};
		VkPhysicalDeviceSubgroupProperties subgroup_properties{};
		VkPhysicalDeviceDescriptorIndexingProperties descriptor_indexing_properties{};
        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptor_buffer_properties{};

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

        VkInstance instance{VK_NULL_HANDLE};
        VkDevice device{VK_NULL_HANDLE};
        VkPhysicalDevice physical_device{VK_NULL_HANDLE};
        VmaAllocator allocator{VK_NULL_HANDLE};
        VkAllocationCallbacks* allocation_callbacks{};

        VkDebugUtilsMessengerEXT debug_utils_messenger{VK_NULL_HANDLE};

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
        std::shared_ptr<ResourcePool<VulkanBuffer>> buffer_pool{};
        std::shared_ptr<ResourcePool<VulkanTexture>> texture_pool{};

        std::unordered_map<size_t, std::shared_ptr<VulkanSampler>> samplers{};
        ResourceOwned<VulkanBindlessManager> bindless_manager{};

        ResourceOwned<AsyncUploader> async_uploader_{};

        ResourceOwned<VulkanTimerQueryPool> time_query_pool{};

        std::mutex mutex{};

        uint32_t frame_count{};
        uint32_t num_rendering_frames{};

        VulkanDevice(VulkanDeviceCreateInfo const* info);
        ~VulkanDevice() override;

        auto name() -> std::string override { return "VulkanDevice"; }
        auto backend() -> EBackend override { return EBackend::vulkan; }
        auto new_frame(uint32_t frame_count) -> void override;
        auto create_buffer(std::string_view name, BufferCreateInfo const* info) -> BufferHandle override;
        auto create_texture(std::string_view name, TextureCreateInfo const* info) -> TextureHandle override;
        auto create_sampler(std::string_view name, SamplerCreateInfo const* info) -> SamplerHandle override;
        auto create_swapchain(std::string_view name, SwapchainCreateInfo const* info) -> SwapchainHandle;
        auto create_graphics_pipeline(std::string_view name, GraphicsPipelineCreateInfo const* info) -> GraphicsPipelineHandle override;
        auto create_compute_pipeline(std::string_view name, ComputePipelineCreateInfo const* info) -> ComputePipelineHandle override;
        auto create_shader_module(std::string_view name, ShaderModuleCreateInfo const* info) -> ShaderModuleHandle override;
        auto create_command_encoder(EQueueType queue_type) -> std::shared_ptr<CommandEncoder> override;
        auto create_swapchain(SwapchainCreateInfo const* info) -> SwapchainHandle override;
        auto create_shader_program(ShaderProgramCreateInfo const* info) -> std::shared_ptr<RHIShaderProgram> override;
        auto create_shader_object_layout(slang::ISession* session, slang::TypeLayoutReflection* type_layout) -> std::shared_ptr<ShaderObjectLayout> override;
        auto map_buffer(BufferHandle buffer) -> std::byte* override;
        auto unmap_buffer(BufferHandle buffer) -> void override;
        auto async_uploader() -> AsyncUploader* override { return async_uploader_.get(); }
        auto submit_command_buffers(SubmitInfo* info) -> void override;
        auto current_timeline_value(EQueueType type) -> uint64_t override;
        auto wait_for_queue(EQueueType type) -> void override;

        auto wait_idle() -> void override;

        auto create_timer_query() -> TimerQueryHandle override;
        auto poll_query(RHITimerQuery* query) -> bool override;
        auto get_query_result(RHITimerQuery* query) -> float override;
        auto reset_query(RHITimerQuery* query) -> void override;

        auto queue(EQueueType type) -> VulkanQueue*;
        auto queue_family(EQueueType type) -> uint32_t;
    };
}
