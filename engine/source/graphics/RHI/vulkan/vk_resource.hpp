#pragma once

#include "../RHI_resource.hpp"
#include "../tool/state_tracking.hpp"
#include "../tool/buffer_block.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>
#include <unordered_map>
#include <span>
#include <mutex>
#include <array>
#include <list>
#include <queue>
#include <functional>

namespace cannele::inline graphics::rhi::vk
{
    struct VulkanDevice;
    struct VulkanQueue;
    struct VulkanBuffer;
    struct VulkanTexture;
    struct VulkanCommandPool;
    struct VulkanCommandBuffer;
    struct VulkanCommandManager;
    struct VulkanBufferManager;
    struct VulkanTextureManager;
    struct VulkanGraphicsPipeline;
    struct VulkanComputePipeline;
    struct VulkanTimerQueryPool;

    template <typename T, typename PooledResourceType>
    struct PooledResource: T
    {
        std::function<auto (PooledResourceType* resource) -> void> deleter{};

        size_t pool_hash{};
        bool mark_free{false};
    };

    template <typename T, typename U>
    auto assert_ref_count_cast(RefCountPtr<U> u) -> RefCountPtr<T>
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

    template <typename T, typename U> requires (!std::is_pointer_v<U>)
    auto assert_cast(U* u) -> T*
    {
        static_assert(!std::is_same_v<T, U>, "Redundant cast between same types");

#if CNE_DEBUG
        if (!u) return nullptr;
        auto t = dynamic_cast<T*>(u);
        CNE_ASSERT_WITH(t, "Invalid cast.");
        return t;
#else
        return (T*) u;
#endif
    }

    template <typename T>
    struct VulkanDeviceChild
    {
        using ChildType = T;
        VulkanDevice* parent{};

        VulkanDeviceChild(VulkanDevice* device)
            : parent(device) {}
    };

    struct VulkanBufferView final
    {
        EDescriptorType resource_type{EDescriptorType::last};
        BufferRange range{};
        uint32_t bindless_index{k_invalid_bindless_index};
    };

    struct VulkanBuffer final: PooledResource<RHIBuffer, VulkanBuffer>, VulkanDeviceChild<VulkanBuffer>
    {
        using PoolType = VulkanBuffer;
        using BufferViewKey = uint32_t;

        size_t allocated_size_bytes{};
        BufferCreateInfo info{};
        BufferStateTracker tracker{};

        VkBuffer buffer{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        VkDeviceAddress device_address{~0ull};
        std::unordered_map<BufferViewKey, VulkanBufferView> buffer_views{};

        void* mapped_ptr{};

        VulkanBuffer(VulkanDevice* device, BufferCreateInfo* info);
        ~VulkanBuffer();

        auto description() -> BufferCreateInfo const* override { return &info; }
        auto bindless_index(BufferRange range, EDescriptorType type) -> uint32_t override;

        auto map() -> void*;
        auto unmap() -> void;

        // TODO: vma flush and invalidate.

        template <typename T>
        auto map() -> T* { return (T*) map(); }
    };

    struct VulkanTextureView final
    {
        VkImageView image_view{VK_NULL_HANDLE};
        EDescriptorType resource_type{EDescriptorType::last};
        uint32_t bindless_index{k_invalid_bindless_index};
    };

    struct VulkanTexture final: PooledResource<RHITexture, VulkanTexture>, VulkanDeviceChild<VulkanTexture>
    {
        using PoolType = VulkanTexture;
        using ImageViewKey = uint32_t;
        using TextureViewKey = uint32_t;

        TextureCreateInfo info{};
        TextureStateTracker tracker{};

        VkImage image{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        std::unordered_map<TextureViewKey, VulkanTextureView> texture_views{};
        std::unordered_map<ImageViewKey, VkImageView> image_views{};

        explicit VulkanTexture(VulkanDevice* device, TextureCreateInfo* info);
        explicit VulkanTexture(VulkanDevice* device, TextureCreateInfo* info, VkImage in_image);
        ~VulkanTexture();

        auto description() -> TextureCreateInfo const* override { return &info; }
        auto bindless_index(TextureSubresourceSet subresources, EDescriptorType type) -> uint32_t override;

        auto image_view_type() -> VkImageViewType;
        auto image_view(TextureSubresourceSet subresources) -> VkImageView;
    };

    struct VulkanSampler final: RHISampler, VulkanDeviceChild<VulkanSampler>
    {
        SamplerCreateInfo info{};

        VkSampler sampler{VK_NULL_HANDLE};

        uint32_t bindless_idx{k_invalid_bindless_index};

        VulkanSampler(VulkanDevice* device, SamplerCreateInfo* info);
        ~VulkanSampler();

        auto description() -> SamplerCreateInfo const* override { return &info; }
        auto bindless_index() -> uint32_t override;
    };

    struct SwapchainSupportDetails final
    {
        std::vector<VkSurfaceFormatKHR> surface_formats{};
        std::vector<VkPresentModeKHR> present_modes{};
    };

    struct VulkanSwapchain final: RHISwapchain, VulkanDeviceChild<VulkanSwapchain>
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
        std::vector<RefCountPtr<VulkanTexture>> backbuffers{};
        std::vector<VkSemaphore> backbuffer_ready_semaphores{};
        std::vector<VkSemaphore> render_finished_semaphores{};
        std::vector<uint64_t> last_submition_times{};

        VulkanSwapchain(VulkanDevice* device, SwapchainCreateInfo* info);
        ~VulkanSwapchain();

        auto acquire_next_backbuffer() -> TextureHandle override;
        auto present(uint64_t submission_time) -> void override;
        auto enqueue_backbuffer_ready_wait_semaphore() -> void override;
        auto enqueue_render_finish_signal_semaphore() -> void override;
        auto backbuffer() -> TextureHandle override { return backbuffers[image_index]; }
        auto num_backbuffers() -> uint32_t override { return backbuffers.size(); }
        auto backbuffer_index() -> uint32_t override { return image_index; }

        auto backbuffer_ready_semaphore() -> VkSemaphore { return backbuffer_ready_semaphores[frame_index]; }
        auto render_finished_semaphore() -> VkSemaphore { return render_finished_semaphores[frame_index]; }

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

    struct VulkanTimerQueryPool final: VulkanDeviceChild<VulkanTimerQueryPool>
    {
        VkQueryPool query_pool{VK_NULL_HANDLE};

        int next_available_index{0};
        std::vector<bool> allocated{};
        std::mutex mutex{};

        VulkanTimerQueryPool(VulkanDevice* device);
        ~VulkanTimerQueryPool();

        auto allocate() -> RefCountPtr<VulkanTimerQuery>;
        auto release(int index) -> void;
        auto reset_query(int begin_index, int count) -> void;

        [[nodiscard]] auto capacity() const -> size_t { return allocated.size(); }
    };

    struct VulkanGraphicsPipeline final: RHIGraphicsPipeline, VulkanDeviceChild<VulkanGraphicsPipeline>
    {
        VkPipeline pipeline{VK_NULL_HANDLE};
        VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};

        VulkanGraphicsPipeline(VulkanDevice* device, GraphicsPipelineCreateInfo* info);
        ~VulkanGraphicsPipeline();
    };

    struct VulkanComputePipeline final: RHIComputePipeline, VulkanDeviceChild<VulkanComputePipeline>
    {
        VkPipeline pipeline{VK_NULL_HANDLE};
        VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};

        VulkanComputePipeline(VulkanDevice* device, ComputePipelineCreateInfo* info);
        ~VulkanComputePipeline();
    };

    struct VulkanShaderModule final: RHIShaderModule, VulkanDeviceChild<VulkanShaderModule>
    {
        VkShaderModule shader_module{VK_NULL_HANDLE};

        VkShaderStageFlagBits stage{};
        uint32_t push_constant_size{};
        std::string entry_point{};

        VulkanShaderModule(VulkanDevice* device, ShaderModuleCreateInfo* info);
        ~VulkanShaderModule();

        auto recreate(std::span<std::byte> code) -> void override;
        auto entry() -> std::string_view override;
        auto create_module(std::span<std::byte> code) -> void;
    };

    struct VulkanCommandBuffer final: VulkanDeviceChild<VulkanCommandBuffer>
    {
        VkCommandBuffer command_buffer{VK_NULL_HANDLE};
        VkCommandPool command_pool{VK_NULL_HANDLE};

        std::vector<RefCountPtr<IResource>> referenced_resources{};
        std::vector<RefCountPtr<VulkanBuffer>> referenced_sataging_buffers{};

        uint64_t recording_time{0};
        uint64_t submission_time{0};

        VulkanCommandBuffer(VulkanDevice* device, uint32_t queue_family_index);
        ~VulkanCommandBuffer();

        auto add_reference(RefCountPtr<IResource> resource) -> void;
        auto add_reference_sataging_buffer(RefCountPtr<VulkanBuffer> buffer) -> void;

        auto reset() -> void;
        auto clear_references() -> void;
    };

    using VulkanCommandBufferPtr = std::shared_ptr<VulkanCommandBuffer>;

    struct VulkanCommandList final: RHICommandList, VulkanDeviceChild<VulkanCommandList>
    {
        CommandListCreateInfo info{};

        VulkanCommandBufferPtr active_command_buffer{};
        ResourceStateTracker resource_state_tracker{};

        VkPipelineLayout current_pipeline_layout{VK_NULL_HANDLE};
        VkShaderStageFlags current_push_constant_visibility{};
        GraphicsState current_graphics_state{};
        ComputeState current_compute_state{};
        bool automatic_barriers{true};

        BufferBlockPool* block_pool{};

        VulkanCommandList(VulkanDevice* device, CommandListCreateInfo* info);
        ~VulkanCommandList();

        auto start() -> void override;
        auto finish() -> void override;
        auto reset() -> void override;

        auto clear_buffer_uint(BufferHandle buffer, uint32_t clear_value) -> void override;
        auto clear_texture_float(TextureHandle texture, TextureSubresourceSet subresources, math::float4 clear_color) -> void override;
        auto clear_depth_stencil(TextureHandle texture, TextureSubresourceSet subresources, std::optional<float> clear_depth, std::optional<uint8_t> clear_stencil) -> void override;
        auto clear_texture_uint(TextureHandle texture, TextureSubresourceSet subresources, uint32_t clear_color) -> void override;

        auto copy_buffer(BufferHandle src_buffer, size_t src_offset_byte, BufferHandle dst_buffer, size_t dst_offset_byte, size_t data_size_byte) -> void override;
        auto copy_texture(TextureHandle src_texture, TextureSlice src_slice, TextureHandle dst_texture, TextureSlice dst_slice) -> void override;
        auto write_buffer(BufferHandle buffer, std::span<std::byte> data, size_t offset_byte) -> void override;
        auto write_texture(TextureHandle texture, uint32_t level, uint32_t layer, TextureSliceDataView data) -> void override;

        auto push_constants(std::span<std::byte> data) -> void override;
        auto set_compute_state(ComputeState* state) -> void override;
        auto dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) -> void override;
        auto dispatch_indirect(BufferHandle buffer, uint32_t offset) -> void override;

        auto set_graphics_state(GraphicsState* state) -> void override;
        auto set_viewport_state(ViewportState* state) -> void override;
        auto draw(DrawArguments* args) -> void override;
        auto draw_indexed(DrawArguments* args) -> void override;
        auto draw_indirect(uint32_t offset_bytes, uint32_t draw_count) -> void override;
        auto draw_indexed_indirect(uint32_t offset_bytes, uint32_t draw_count) -> void override;

        auto push_command_label(std::string_view name, math::float4 color) -> void override;
        auto pop_command_label() -> void override;

        auto begin_timestep(RHITimerQuery* query) -> void override;
        auto end_timestep(RHITimerQuery* query) -> void override;

        auto enbale_automatic_barriers(bool enable) -> void override;
        auto begin_tracking_buffer(BufferHandle buffer, EResourceStates current_state) -> void override;
        auto begin_tracking_texture(TextureHandle texture, TextureSubresourceSet subresources, EResourceStates current_state) -> void override;
        auto set_buffer_state(BufferHandle buffer, EResourceStates dst_state) -> void override;
        auto set_texture_state(TextureHandle texture, TextureSubresourceSet subresources, EResourceStates dst_state) -> void override;
        auto lock_buffer_state(BufferHandle buffer, EResourceStates dst_state) -> void override;
        auto lock_texture_state(TextureHandle texture, EResourceStates dst_state) -> void override;

        auto flush() -> void override;

        auto buffer_state(BufferHandle buffer) -> EResourceStates override;
        auto texture_state(TextureHandle texture, uint32_t level, uint32_t layer) -> EResourceStates override;

        auto commit_barriers(EQueueType src_queue = EQueueType::ignore, EQueueType dst_queue = EQueueType::ignore) -> void override;

        auto wait_for_submit(EQueueType submit_queue_type, uint64_t submit_time) -> void override;

        auto device() -> IDevice* override;

        auto finish_submission(VulkanQueue* queue, uint64_t submission_time) -> void;

        auto end_rendering() -> void;

        auto command_buffer() -> VkCommandBuffer { return active_command_buffer->command_buffer; }

        auto clear_texture(TextureHandle texture, TextureSubresourceSet subresources, VkClearColorValue* clear_color) -> void;

        auto set_dynamic_state() -> void;
    };

    struct VulkanQueue final: VulkanDeviceChild<VulkanQueue>
    {
        EQueueType type{EQueueType::graphics};
        VkQueue queue{VK_NULL_HANDLE};
        uint32_t family_index{(uint32_t) -1};

        std::mutex mutex{};
        VkSemaphore timeline{VK_NULL_HANDLE};
        struct WaitSemaphoreInfo final
        {
            VkSemaphore semaphore{VK_NULL_HANDLE};
            uint64_t value{0};
            VkPipelineStageFlags2 wait_stage{VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT};
        };
        std::vector<WaitSemaphoreInfo> wait_semaphores{};
        std::vector<std::pair<VkSemaphore, uint64_t>> signal_semaphores{};

        std::unique_ptr<BufferBlockPool> buffer_block{};

        // Recording time: time point of the command list start.
        // Submission time: signal time point of the queue submission.
        // Completion time: time point of the submission completed, the same as timeline fence signal time point, need to update before use.
        uint64_t last_recording_time{0};
        uint64_t last_submitted_time{0};
        uint64_t last_completion_time{0};

        std::list<std::shared_ptr<VulkanCommandBuffer>> command_buffers_in_flight{};
        std::list<std::shared_ptr<VulkanCommandBuffer>> command_buffers_free{};

        VulkanQueue(VulkanDevice* device, EQueueType type, uint32_t family_index, VkQueue queue);
        ~VulkanQueue();

        auto allocate_command_buffer() -> VulkanCommandBufferPtr;

        auto add_wait_semaphore(VkSemaphore semaphore, uint64_t value, VkPipelineStageFlags2 opt_wait_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT) -> void;
        auto add_signal_semaphore(VkSemaphore semaphore, uint64_t value) -> void;

        auto submit(std::span<VulkanCommandList*> command_lists) -> uint64_t;

        auto refresh_command_buffers() -> void;

        auto command_buffer_in_flight(uint64_t submission_time) -> VulkanCommandBufferPtr;

        auto update_last_completion_time() -> uint64_t;

        auto poll_command_list(uint64_t submission_time) -> bool;
        auto wait_command_list(uint64_t submission_time, uint64_t timeout) -> bool;
    };

    // Managers are subsystems of the device, they help device to manage specific resources.

    struct VulkanLayoutManager final: VulkanDeviceChild<VulkanLayoutManager>
    {
        using PipelineLayoutKey = size_t;
        using DescriptorSetLayoutKey = size_t;

        std::unordered_map<PipelineLayoutKey, VkPipelineLayout> pipeline_layouts{};
        std::unordered_map<DescriptorSetLayoutKey, VkDescriptorSetLayout> descriptor_set_layouts{};

        VulkanLayoutManager(VulkanDevice* device);
        ~VulkanLayoutManager();

        auto create_descriptor_set_layout(std::span<VkDescriptorSetLayoutBinding> bindings) -> VkDescriptorSetLayout;
        auto create_pipeline_layout(std::span<VkDescriptorSetLayout> set_layouts, std::span<VkPushConstantRange> push_constant_ranges) -> VkPipelineLayout;
    };

    struct VulkanPipelineManager final: VulkanDeviceChild<VulkanPipelineManager>
    {
        using PipelineKey = size_t;
        using ShaderKey = size_t;

        std::unordered_map<PipelineKey, RefCountPtr<VulkanGraphicsPipeline>> graphics_pipelines{};
        std::unordered_map<PipelineKey, RefCountPtr<VulkanComputePipeline>> compute_pipelines{};
        std::unordered_map<ShaderKey, RefCountPtr<VulkanShaderModule>> shader_modules{};

        std::mutex mutex{};

        VulkanPipelineManager(VulkanDevice* device);
        ~VulkanPipelineManager();

        auto create_graphics_pipeline(std::string_view name, GraphicsPipelineCreateInfo* info) -> RefCountPtr<VulkanGraphicsPipeline>;
        auto create_compute_pipeline(std::string_view name, ComputePipelineCreateInfo* info) -> RefCountPtr<VulkanComputePipeline>;
        auto create_shader_module(ShaderModuleCreateInfo* info) -> VulkanShaderModule*;
    };

    struct VulkanBindlessManager final: VulkanDeviceChild<VulkanBindlessManager>
    {
        CNE_MOVE_ONLY(VulkanBindlessManager);

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
