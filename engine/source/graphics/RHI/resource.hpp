#pragma once

#include "forward.hpp"
#include "command_list.hpp"
#include "shader.hpp"

#include <core/enum_flag.hpp>
#include <core/hash.hpp>
#include <core/ref_count_ptr.hpp>
#include <core/inplace_vector.hpp>
#include <math/type.hpp>
#include <platform/shader_compile.hpp>

#include <cstdint>
#include <vector>
#include <mdspan>

namespace cannele::inline graphics::rhi
{
    struct ResourcePoolBase: std::enable_shared_from_this<ResourcePoolBase>
    {
        CNE_INTERFACE(ResourcePoolBase);

        enum struct PoolState: uint8_t
        {
            usable,
            releasing,
        };
        PoolState state{};

        virtual auto recycle_resource(IPoolableResource* resource) -> void = 0;
    };

    struct IPoolableResource: IResource
    {
        CNE_INTERFACE(IPoolableResource);

        using PoolState = ResourcePoolBase::PoolState;
        using IResource::IResource;

        std::weak_ptr<ResourcePoolBase> pool{};
        size_t pool_hash{};

        auto delete_this() -> void
        {
            if (auto locked_pool = pool.lock(); locked_pool && locked_pool->state == PoolState::usable) {
                locked_pool->recycle_resource(this);
            } else {
                CNE_TRACE("Delete resource: {} at hash {}", (void*) this, pool_hash);
                delete this;
            }
        }
    };

    enum struct EBufferUsage: uint16_t
    {
        none         = 0,
        transfer_src = 1 << 0,
        transfer_dst = 1 << 1,
        vertex       = 1 << 2,
        index        = 1 << 3,
        uniform      = 1 << 4,
        storage      = 1 << 5,
        indirect     = 1 << 6,
    };
    ENUM_STRUCT_FLAGS(EBufferUsage);

    enum struct EMemoryType: uint8_t
    {
        gpu_only,
        cpu_read,
        cpu_upload,
    };

    struct BufferCreateInfo
    {
        EMemoryType memory_type{EMemoryType::gpu_only};
        EBufferUsage usage{EBufferUsage::none};
        size_t size_bytes{};
        uint32_t stride{};
        EResourceStates final_state{EResourceStates::unknown};

        auto operator <=> (BufferCreateInfo const& other) const = default;
    };

    struct RHIBuffer: IPoolableResource
    {
        CNE_INTERFACE(RHIBuffer);
        using IPoolableResource::IPoolableResource;

        using Description = BufferCreateInfo;

        virtual auto description() -> Description const* = 0;
        // Require a buffer range bindless index, EDescriptorType must be uniform_buffer or storage_buffer.
        // If usage not contains the type need, will get an invalid bindless index.
        virtual auto descriptor_handle(
            BufferRange range = {},
            EDescriptorType type = EDescriptorType::storage_buffer
        ) -> math::uint2 = 0;

        auto resolve_range(BufferRange const& range) -> BufferRange;
    };

    auto adapt_to_buffer(BufferRange* range, BufferCreateInfo const* info) -> void;

    enum struct ETextureDimension: uint8_t
    {
        tex_2d,
        tex_2d_array,
        tex_3d,
        tex_cube,
        tex_cube_array,
    };

    enum struct ETextureUsage: uint16_t
    {
        none                     = 0,
        transfer_src             = 1 << 0,
        transfer_dst             = 1 << 1,
        resolve_src              = 1 << 2,
        resolve_dst              = 1 << 3,
        sampled                  = 1 << 4,
        storage                  = 1 << 5,
        color_attachment         = 1 << 6,
        depth_stencil_attachment = 1 << 7,
        present                  = 1 << 8,
    };
    ENUM_STRUCT_FLAGS(ETextureUsage);

    struct TextureCreateInfo
    {
        ETextureDimension dimension{ETextureDimension::tex_2d};
        EFormat format{EFormat::undefined};
        ETextureUsage usage{ETextureUsage::none};
        Extent3D extent{1, 1, 1};
        uint32_t mip_count{1};
        uint32_t layer_count{1};
        uint32_t sample_count{1};
        EResourceStates final_state{EResourceStates::unknown};

        auto operator == (TextureCreateInfo const& other) const -> bool = default;
    };
    static constexpr auto k_remaining_texture_size = 0xffffffffu;

    auto depth_attachment_create_info(math::uint2 extent, EFormat format) -> TextureCreateInfo;
    auto contain_all_resources(TextureSubresourceRange* subresources, TextureCreateInfo const* info) -> bool;
    auto adapt_to_texture(TextureSubresourceRange* subresources, TextureCreateInfo const* info, bool signle_mip_level) -> void;

    struct RHITexture: IPoolableResource
    {
        CNE_INTERFACE(RHITexture);
        using IPoolableResource::IPoolableResource;

        using Description = TextureCreateInfo;

        virtual auto description() -> Description const* = 0;
        virtual auto view(TextureSubresourceRange const& subresources = k_all_subresources) -> RHITextureView* = 0;

        auto resolve_subresource_rage(TextureSubresourceRange const& subresources) -> TextureSubresourceRange;
        auto contains_all_subresources(TextureSubresourceRange const& subresources) -> bool;
    };

    struct RHITextureView
    {
        CNE_INTERFACE(RHITextureView);

        virtual auto texture() -> RHITexture* = 0;
        virtual auto range() -> TextureSubresourceRange = 0;
        virtual auto descriptor_handle(EDescriptorType type = EDescriptorType::sampled_texture) -> math::uint2 = 0;
    };

    struct SamplerCreateInfo final
    {
        ESamplerFilter filter_min{ESamplerFilter::linear};
        ESamplerFilter filter_mag{ESamplerFilter::linear};
        ESamplerFilter filter_mip{ESamplerFilter::linear};
        ESamplerAddressMode address_u{ESamplerAddressMode::repeat};
        ESamplerAddressMode address_v{ESamplerAddressMode::repeat};
        ESamplerAddressMode address_w{ESamplerAddressMode::repeat};
        ECompareOperation compare_operation{ECompareOperation::never};
        float anisotropy{0.0f};
        float mip_bias{0.0f};
        float min_mip_level{0.0f};
        float max_mip_level{FLT_MAX};
        uint32_t border_color{0};

        auto operator <=> (SamplerCreateInfo const& other) const = default;
    };

    struct RHISampler: IResource
    {
        CNE_INTERFACE(RHISampler);
        using IResource::IResource;

        using Description = SamplerCreateInfo;

        virtual auto description() -> Description const* = 0;
        virtual auto descriptor_handle() -> math::uint2 = 0;
    };

    struct ShaderModuleCreateInfo final
    {
        std::string_view name{};
        std::string_view file_name{};
        std::string_view entry{};
        EShaderStage stage{};
        std::vector<std::byte> code{};

        auto operator <=> (ShaderModuleCreateInfo const& other) const = default;
    };

    struct RHIShaderModule: IResource
    {
        CNE_INTERFACE(RHIShaderModule);
        using IResource::IResource;

        virtual auto recreate(std::span<std::byte const> code) -> void = 0;
        virtual auto entry() -> std::string_view = 0;
    };

    struct VertexBufferBinding final
    {
        BufferHandle buffer{};
        uint32_t slot{};
        size_t offset_bytes{};

        auto operator <=> (VertexBufferBinding const& other) const = default;
    };

    struct IndexBufferBinding final
    {
        BufferHandle buffer{};
        EFormat format{};
        size_t offset_bytes{};

        auto operator <=> (IndexBufferBinding const& other) const = default;

        explicit operator bool () noexcept
        {
            return buffer != nullptr;
        }
    };

    struct GraphicsPipelineCreateInfo final
    {
        ShaderProgramHandle program{};

        std::vector<ColorAttachmentInfo> colors{};
        DepthStencilAttachmentInfo depth_stencil{};
        ERasterizerTopologyType topology{ERasterizerTopologyType::triangle_list};

        // All dynamic state, when set to false, should provide in the create info.
        // Viewport and scissor are always dynamic.
        bool dynamic_input_state    : 1{true};
        bool dynamic_blend_states   : 1{true};
        bool dynamic_depth_state    : 1{true};
        bool dynamic_stencil_state  : 1{true};
        std::optional<VertexInputState> input_state{};
    };

    struct IPipeline: IResource
    {
        CNE_INTERFACE(IPipeline);
        using IResource::IResource;

        virtual auto program() const -> RHIShaderProgram const* = 0;
    };

    // PSO: Pipeline State Object
    struct RHIGraphicsPipeline: IPipeline
    {
        CNE_INTERFACE(RHIGraphicsPipeline);
        using IPipeline::IPipeline;
    };

    struct ComputePipelineCreateInfo final
    {
        ShaderProgramHandle program{};
    };

    struct RHIComputePipeline: IPipeline
    {
        CNE_INTERFACE(RHIComputePipeline);
        using IPipeline::IPipeline;
    };

    struct RayTracingPipelineCreateInfo final
    {
        ShaderModuleHandle rgen{};
        ShaderModuleHandle rchit{};
        ShaderModuleHandle rmiss{};

        size_t max_recursion_depth{1};
    };

    struct RHIRayTracingPipeline: IPipeline
    {
        CNE_INTERFACE(RHIRayTracingPipeline);
        using IPipeline::IPipeline;
    };

    struct SwapchainCreateInfo final
    {
        uint32_t width{1};
        uint32_t height{1};
        void* window_handle{};

        EPresentMode present_mode{EPresentMode::immediate};
        EFormat surface_format{EFormat::rgba8_unorm};
        EColorSpace color_space{EColorSpace::srgb_nonliner};

        auto operator <=> (SwapchainCreateInfo const& other) const = default;
    };

    struct RHISwapchain: IResource
    {
        CNE_INTERFACE(RHISwapchain);
        using IResource::IResource;

        virtual auto num_backbuffers() -> uint32_t = 0;
        virtual auto acquire_next_backbuffer() -> TextureHandle = 0;
        virtual auto present() -> void = 0;
    };

    struct RHITimerQuery: IResource
    {
        CNE_INTERFACE(RHITimerQuery);
        using IResource::IResource;
    };

    struct DispatchIndirectCommand final
    {
        uint32_t x{};
        uint32_t y{};
        uint32_t z{};

        auto operator <=> (DispatchIndirectCommand const& other) const = default;
    };

    struct DrawIndirectCommand final
    {
        uint32_t vertex_count{};
        uint32_t instance_count{};
        uint32_t first_vertex{};
        uint32_t first_instance{};

        auto operator <=> (DrawIndirectCommand const& other) const = default;
    };

    struct DrawIndexedIndirectCommand final
    {
        uint32_t index_count{};
        uint32_t instance_count{};
        uint32_t first_index{};
        int32_t  vertex_offset{};
        uint32_t first_instance{};

        auto operator <=> (DrawIndexedIndirectCommand const& other) const = default;
    };

    // CommandBuffer holds all data.
    struct RHICommandBuffer: IResource
    {
        CNE_INTERFACE(RHICommandBuffer);

        std::unique_ptr<Arena> arena{};
        std::unordered_set<std::shared_ptr<IResource>> tracked_resources{};
        std::unique_ptr<CommandList> command_list{};

        RHICommandBuffer(IDevice* device);

        virtual auto reset() -> void
        {
            command_list->reset();
            tracked_resources.clear();
            arena->reset();
        }
    };

    struct GraphicsCommandEncoder;
    struct ComputeCommandEncoder;

    struct CommandEncoder: IResource
    {
        CNE_INTERFACE(CommandEncoder);
        CommandEncoder(IDevice* device);

        using TrackedResources = std::unordered_set<std::shared_ptr<IResource>>;
        Arena* arena{};
        TrackedResources* tracked_resources{};
        CommandList* command_list{};

        std::unique_ptr<GraphicsCommandEncoder> graphics_encoder{};
        std::unique_ptr<ComputeCommandEncoder> compute_encoder{};

        auto copy_buffer(
            BufferHandle src_buffer,
            size_t src_offset,
            BufferHandle dst_buffer,
            size_t dst_offset,
            size_t size
        ) -> void;

        auto copy_texture(
            TextureHandle src_texture,
            TextureSubresourceRange src_subresources,
            Offset3D src_offset,
            TextureHandle dst_texture,
            TextureSubresourceRange dst_subresources,
            Offset3D dst_offset,
            Extent3D extent
        ) -> void;

        auto upload_texture_data(
            TextureHandle texture,
            TextureSliceDataView data,
            TextureSubresourceRange subresources = k_all_subresources,
            Offset3D offset = {},
            Extent3D extent = k_whole_extent
        ) -> void;

        auto upload_buffer_data(
            BufferHandle buffer,
            size_t offset,
            std::span<std::byte const> data
        ) -> void;

        auto clear_buffer_uint(
            BufferHandle buffer,
            BufferRange range = {},
            uint32_t clear_value = 0
        ) -> void;

        auto clear_texture_uint(
            TextureHandle texture,
            TextureSubresourceRange subresources,
            math::uint4 clear_color
        ) -> void;

        auto clear_texture_float(
            TextureHandle texture,
            TextureSubresourceRange subresources,
            math::float4 clear_color
        ) -> void;

        auto clear_texture_depth_stencil(
            TextureHandle texture,
            TextureSubresourceRange subresources,
            std::optional<float> clear_depth,
            std::optional<uint8_t> clear_stencil
        ) -> void;

        auto resolve_query(
            TimerQueryHandle query,
            uint32_t query_index,
            BufferHandle buffer,
            size_t offset,
            uint32_t query_count = 1
        ) -> void;

        auto set_buffer_state(
            BufferHandle buffer,
            EResourceStates state
        ) -> void;

        auto set_texture_state(
            TextureHandle texture,
            TextureSubresourceRange subresources,
            EResourceStates state
        ) -> void;

        auto commit_barriers() -> void;

        auto push_debug_label(std::string_view name, math::float4 color = math::float4{1.0f}) -> void;

        auto pop_debug_label() -> void;

        auto insert_debug_marker(std::string_view name, math::float4 color = math::float4{1.0f}) -> void;

        auto write_timestamp(TimerQueryHandle query, uint32_t query_index) -> void;

        auto begin_graphics_pass(
            std::span<ColorAttachment> color_attachments,
            std::optional<DepthStencilAttachment> depth_stencil_attachment = {}
        ) -> GraphicsCommandEncoder*;

        auto begin_compute_pass() -> ComputeCommandEncoder*;

        // When finished the command encoder, the command encoder can not be used anymore.
        virtual auto finish() -> std::shared_ptr<RHICommandBuffer> = 0;
        virtual auto binding_data(RootShaderObject* root_object) -> BindingData* = 0;
    };

    using CommandEncoderHandle = std::shared_ptr<CommandEncoder>;

    struct GraphicsCommandEncoder
    {
        CommandEncoder* encoder{};
        CommandList* command_list{};
        GraphicsPipelineHandle pipeline{};
        RootShaderObjectHandle root_object{};

        GraphicsCommandEncoder(CommandEncoder* encoder);

        auto bind_pipeline(GraphicsPipelineHandle pipeline) -> ShaderObject*;

        auto set_graphics_state(GraphicsState state) -> void;

        auto draw(DrawArguments const& args) -> void;
        auto draw_indexed(DrawArguments const& args) -> void;
        auto draw_indirect(BufferHandle indirect_buffer, uint32_t offset, uint32_t draw_count = 1) -> void;
        auto draw_indexed_indirect(BufferHandle indirect_buffer, uint32_t offset, uint32_t draw_count = 1) -> void;
        auto dispatch_mesh(uint32_t group_count_x, uint32_t group_count_y = 1, uint32_t group_count_z = 1) -> void;
        auto dispatch_mesh_indirect(BufferHandle indirect_buffer, uint32_t offset = 0, uint32_t count = 1) -> void;

        auto push_command_label(std::string_view name, math::float4 color) -> void;
        auto pop_command_label() -> void;
        auto insert_debug_marker(std::string_view name, math::float4 color) -> void;

        auto write_timestamp(TimerQueryHandle query, uint32_t query_index) -> void;

        auto finish() -> void;
    };

    struct ComputeCommandEncoder
    {
        CommandEncoder* encoder{};
        CommandList* command_list{};
        ComputePipelineHandle pipeline{};
        RootShaderObjectHandle root_object{};

        ComputeCommandEncoder(CommandEncoder* encoder);

        auto bind_pipeline(ComputePipelineHandle pipeline) -> ShaderObject*;

        auto set_compute_state() -> void;

        auto dispatch(uint32_t group_count_x, uint32_t group_count_y = 1, uint32_t group_count_z = 1) -> void;
        auto dispatch_indirect(BufferHandle indirect_buffer, uint32_t offset = 0) -> void;

        auto push_command_label(std::string_view name, math::float4 color) -> void;
        auto pop_command_label() -> void;
        auto insert_debug_marker(std::string_view name, math::float4 color) -> void;

        auto write_timestamp(TimerQueryHandle query, uint32_t query_index) -> void;

        auto finish() -> void;
    };

    struct SubmitInfo
    {
        EQueueType queue_type{EQueueType::graphics};
        std::span<std::shared_ptr<RHICommandBuffer>> command_buffers{};
        // TODO: Support external semaphores.
    };
}

namespace cannele::inline graphics::rhi
{

}
