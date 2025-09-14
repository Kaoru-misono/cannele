#pragma once

#include "vk_forward.hpp"
#include "vk_command.hpp"
#include "../resource.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>
#include <unordered_map>
#include <span>
#include <mutex>
#include <array>
#include <list>
#include <queue>

namespace cannele::inline graphics::rhi::vk
{
    struct VulkanBufferView final
    {
        EDescriptorType resource_type{EDescriptorType::last};
        BufferRange range{};
        uint32_t bindless_index{k_invalid_bindless_index};
    };

    struct VulkanBuffer final: RHIBuffer
    {
        using PoolType = VulkanBuffer;
        using BufferViewKey = uint32_t;

        size_t allocated_size_bytes{};
        BufferCreateInfo info{};

        VkBuffer buffer{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        VkDeviceAddress device_address{~0ull};
        std::unordered_map<BufferViewKey, VulkanBufferView> buffer_views{};

        void* mapped_ptr{};

        VulkanBuffer(VulkanDevice* device, BufferCreateInfo const* info);
        ~VulkanBuffer();

        auto description() -> BufferCreateInfo const* override { return &info; }
        auto descriptor_handle(BufferRange range, EDescriptorType type) -> math::uint2 override;

        // TODO: vma flush and invalidate.
    };

    struct VulkanTextureView final: RHITextureView
    {
        VulkanTexture* texture_{};
        TextureSubresourceRange range_{};
        VkImageView image_view{VK_NULL_HANDLE};
        VkImageLayout image_layout{VK_IMAGE_LAYOUT_UNDEFINED};
        // 0: Read-only, 1 : Read-write // TODO: Descriptor Handle support.
        uint32_t bindless_index[2]{k_invalid_bindless_index};

        auto texture() -> RHITexture* override { return (RHITexture*) texture_; }
        auto range() -> TextureSubresourceRange override { return range_; }
        auto descriptor_handle(EDescriptorType type) -> math::uint2 override;
    };

    struct VulkanTexture final: RHITexture
    {
        using PoolType = VulkanTexture;
        using ImageViewKey = uint32_t;
        using TextureViewKey = uint32_t;

        TextureCreateInfo info{};

        VkImage image{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        VkFormat format{VK_FORMAT_UNDEFINED};
        std::unordered_map<ImageViewKey, VulkanTextureView> texture_subresource_views{};
        VulkanTextureView* default_view_{};

        explicit VulkanTexture(VulkanDevice* device, TextureCreateInfo const* info);
        explicit VulkanTexture(VulkanDevice* device, TextureCreateInfo const* info, VkImage in_image);
        ~VulkanTexture();

        auto description() -> TextureCreateInfo const* override { return &info; }
        auto view(TextureSubresourceRange const& subresources) -> RHITextureView* override;

        auto image_view_type() -> VkImageViewType;
        auto subresource_view(TextureSubresourceRange const& subresources) -> VulkanTextureView*;
    };

    struct VulkanSampler final: RHISampler
    {
        SamplerCreateInfo info{};

        VkSampler sampler{VK_NULL_HANDLE};

        uint32_t bindless_idx{k_invalid_bindless_index};

        VulkanSampler(VulkanDevice* device, SamplerCreateInfo const* info);
        ~VulkanSampler();

        auto description() -> SamplerCreateInfo const* override { return &info; }
        auto descriptor_handle() -> math::uint2 override;
    };

    struct SwapchainSupportDetails final
    {
        std::vector<VkSurfaceFormatKHR> surface_formats{};
        std::vector<VkPresentModeKHR> present_modes{};
    };

    struct VulkanSwapchain final: RHISwapchain
    {
        VkPresentModeKHR present_mode{VK_PRESENT_MODE_FIFO_KHR};
        VkFormat surface_format{VK_FORMAT_UNDEFINED};
        VkColorSpaceKHR color_space{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        VkExtent2D extent{};

        VkSwapchainKHR swapchain{VK_NULL_HANDLE};
        VkSurfaceKHR surface{VK_NULL_HANDLE};
        SwapchainSupportDetails support_details{};
        uint32_t image_index{};
        uint32_t frame_index{};
        std::vector<std::shared_ptr<VulkanTexture>> backbuffers{};
        std::vector<VkSemaphore> backbuffer_ready_semaphores{};
        std::vector<VkSemaphore> render_finished_semaphores{};
        std::vector<VkFence> last_submition_fences{};

        VulkanSwapchain(VulkanDevice* device, SwapchainCreateInfo const* info);
        ~VulkanSwapchain();

        auto acquire_next_backbuffer() -> TextureHandle override;
        auto present() -> void override;
        auto num_backbuffers() -> uint32_t override { return backbuffers.size(); }

        auto create_swapchain() -> void;
    };

    struct VulkanTimerQuery final: RHITimerQuery
    {
        int begin_index{-1};
        int end_index{-1};

        bool started{false};
        bool resolved{false};
        float time{0.0f};

        VulkanTimerQueryPool* pool{};

        VulkanTimerQuery(VulkanTimerQueryPool* pool, int begin_index, int end_index);
        ~VulkanTimerQuery();
    };

    struct VulkanTimerQueryPool final
    {
        VulkanDevice* parent{};
        VkQueryPool query_pool{VK_NULL_HANDLE};

        int next_available_index{0};
        std::vector<bool> allocated{};
        std::mutex mutex{};

        VulkanTimerQueryPool(VulkanDevice* device);
        ~VulkanTimerQueryPool();

        auto allocate() -> std::shared_ptr<VulkanTimerQuery>;
        auto release(int index) -> void;
        auto reset_query(int begin_index, int count) -> void;

        [[nodiscard]] auto capacity() const -> size_t { return allocated.size(); }
    };

    struct VulkanGraphicsPipeline final: RHIGraphicsPipeline
    {
        GraphicsPipelineCreateInfo info{};

        VkPipeline pipeline{VK_NULL_HANDLE};
        VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};

        VulkanGraphicsPipeline(VulkanDevice* device, GraphicsPipelineCreateInfo const* info);
        ~VulkanGraphicsPipeline();

        auto program() const -> RHIShaderProgram const* override;
    };

    struct VulkanComputePipeline final: RHIComputePipeline
    {
        ComputePipelineCreateInfo info{};

        VkPipeline pipeline{VK_NULL_HANDLE};
        VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};

        VulkanComputePipeline(VulkanDevice* device, ComputePipelineCreateInfo const* info);
        ~VulkanComputePipeline();

        auto program() const -> RHIShaderProgram const* override;
    };

    struct VulkanShaderModule final: RHIShaderModule
    {
        VkShaderModule shader_module{VK_NULL_HANDLE};

        VkShaderStageFlagBits stage{};
        uint32_t push_constant_size{};
        std::string entry_point{};

        VulkanShaderModule(VulkanDevice* device, ShaderModuleCreateInfo const* info);
        ~VulkanShaderModule();

        auto recreate(std::span<std::byte const> code) -> void override;
        auto entry() -> std::string_view override;
        auto create_module(std::span<std::byte const> code) -> void;
    };

    struct VulkanQueue final
    {
        VulkanDevice* parent{};
        EQueueType type{EQueueType::graphics};
        VkQueue queue{VK_NULL_HANDLE};
        uint32_t family_index{(uint32_t) -1};

        std::mutex mutex{};
        VkSemaphore timeline{VK_NULL_HANDLE};

        struct {
            VkFence fence{VK_NULL_HANDLE};
            VkSemaphore image_available_semaphore{VK_NULL_HANDLE};
            VkSemaphore render_finished_semaphore{VK_NULL_HANDLE};

            explicit operator bool() const noexcept { return fence != VK_NULL_HANDLE; }
        } surface_sync{};

        // Submission time: signal time point of the queue submission.
        // Completion time: time point of the submission completed, the same as timeline fence signal time point, need to update before use.
        uint64_t last_submitted_time{0};
        uint64_t last_completion_time{0};

        std::list<std::shared_ptr<VulkanCommandBuffer>> command_buffers_in_flight{};
        std::list<std::shared_ptr<VulkanCommandBuffer>> command_buffers_free{};

        VulkanQueue(VulkanDevice* device, EQueueType type, uint32_t family_index, VkQueue queue);
        ~VulkanQueue();

        auto allocate_command_buffer() -> std::shared_ptr<VulkanCommandBuffer>;
        auto free_command_buffer(std::shared_ptr<VulkanCommandBuffer> command_buffer) -> void;
        auto free_command_buffers() -> void;

        auto create_command_encoder() -> std::shared_ptr<VulkanCommandEncoder>;

        auto submit(SubmitInfo* info) -> uint64_t;
        auto wait() -> bool;

        auto update_last_completion_time() -> uint64_t;
    };

    // Managers are subsystems of the device, they help device to manage specific resources.

    struct VulkanLayoutManager final
    {
        using PipelineLayoutKey = size_t;
        using DescriptorSetLayoutKey = size_t;

        VulkanDevice* parent{};
        std::unordered_map<PipelineLayoutKey, VkPipelineLayout> pipeline_layouts{};
        std::unordered_map<DescriptorSetLayoutKey, VkDescriptorSetLayout> descriptor_set_layouts{};

        VulkanLayoutManager(VulkanDevice* device);
        ~VulkanLayoutManager();

        auto create_descriptor_set_layout(std::span<VkDescriptorSetLayoutBinding> bindings) -> VkDescriptorSetLayout;
        auto create_pipeline_layout(std::span<VkDescriptorSetLayout> set_layouts, std::span<VkPushConstantRange> push_constant_ranges) -> VkPipelineLayout;
    };

    struct VulkanPipelineManager final
    {
        using PipelineKey = size_t;
        using ShaderKey = size_t;

        VulkanDevice* parent{};
        std::unordered_map<PipelineKey, std::shared_ptr<VulkanGraphicsPipeline>> graphics_pipelines{};
        std::unordered_map<PipelineKey, std::shared_ptr<VulkanComputePipeline>> compute_pipelines{};
        std::unordered_map<ShaderKey, std::shared_ptr<VulkanShaderModule>> shader_modules{};

        std::mutex mutex{};

        VulkanPipelineManager(VulkanDevice* device);
        ~VulkanPipelineManager();

        auto create_graphics_pipeline(std::string_view name, GraphicsPipelineCreateInfo const* info) -> std::shared_ptr<VulkanGraphicsPipeline>;
        auto create_compute_pipeline(std::string_view name, ComputePipelineCreateInfo const* info) -> std::shared_ptr<VulkanComputePipeline>;
        auto create_shader_module(ShaderModuleCreateInfo const* info) -> VulkanShaderModule*;
    };

    struct VulkanBindlessManager final
    {
        CNE_MOVE_ONLY(VulkanBindlessManager);

        VulkanDevice* parent{};

        using BindlessIndex = uint32_t;

        static constexpr auto binding_count = (uint32_t) EDescriptorType::last;

        struct BindingInfo final
        {
            VkDescriptorType type{VK_DESCRIPTOR_TYPE_MAX_ENUM};
            uint32_t count{};
            uint32_t limit{};
            size_t descriptor_size{0}; // Size of the descriptor in descriptor buffer.
            VkDeviceSize offset{0};
        };

        std::array<BindingInfo, binding_count> bindings{};

        VkDeviceSize offset_alignment{0};
        size_t stride{0};

        struct DescriptorHeap final
        {
            uint32_t max_descriptors{};
            std::queue<uint32_t> free_indices{};
            uint32_t next_usable_index{0};
            VkDescriptorSetLayout descriptor_set_layout{};
            VkBuffer descriptor_buffer{VK_NULL_HANDLE};
            VmaAllocation descriptor_buffer_allocation{VK_NULL_HANDLE};
            VkDeviceAddress descriptor_buffer_address{0};
            void* descriptor_buffer_mapped_ptr{nullptr};

            std::mutex mutex{};

            auto request_index() -> BindlessIndex;
            auto free_index(BindlessIndex index) -> void;
        };
        std::unique_ptr<DescriptorHeap> resource_heap{};
        std::unique_ptr<DescriptorHeap> sampler_heap{};

        VulkanBindlessManager(VulkanDevice* device);
        ~VulkanBindlessManager();

        [[nodiscard]] auto register_sampler(VkSampler sampler) -> BindlessIndex;
        [[nodiscard]] auto register_buffer(EDescriptorType type, VulkanBuffer* buffer, VkDeviceSize offset, VkDeviceSize range) -> BindlessIndex;
        [[nodiscard]] auto register_texture(EDescriptorType type, VkImageView image_view, VkImageLayout layout) -> BindlessIndex;

        auto free_SRV(BindlessIndex index, VulkanTexture* fallback_texture) -> void;
        auto free_UAV(BindlessIndex index, VulkanTexture* fallback_texture) -> void;
        auto free_UAV(BindlessIndex index, VulkanBuffer* fallback_buffer) -> void;
        auto free_SRV(BindlessIndex index, VulkanBuffer* fallback_buffer) -> void;
    };
}
