#pragma once

#include "definitions.hpp"
#include "forward.hpp"

#include <core/enum_flag.hpp>
#include <core/hash.hpp>
#include <core/ref_count_ptr.hpp>
#include <core/inplace_vector.hpp>
#include <math/type.hpp>
#include <platform/shader_compile.hpp>

#include <cstdint>
#include <vector>
#include <mdspan>
#include <functional>

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

    enum struct EBufferType: uint8_t
    {
        gpu_only,
        cpu_read,
        cpu_write,
    };

    struct BufferCreateInfo
    {
        size_t size_bytes{};
        uint32_t stride{};
        EBufferType type{EBufferType::gpu_only};
        EBufferUsage usage{EBufferUsage::none};

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
    };

    auto adapt_to_buffer(BufferRange* range, BufferCreateInfo const* info) -> void;

    struct BufferBarrier final
    {
        RHIBuffer* buffer{};

        EResourceStates src_state{EResourceStates::unknown};
        EResourceStates dst_state{EResourceStates::unknown};
        EPipelineStage src_stage{EPipelineStage::none};
        EPipelineStage dst_stage{EPipelineStage::none};
    };

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
        math::uint2 extent{1, 1};
        uint32_t depth{1};
        uint32_t num_layers{1};
        uint32_t num_mips{1};
        uint32_t num_samples{1};
        EResourceStates initial_state{EResourceStates::unknown};
        bool keep_initial_state{false};

        auto operator == (TextureCreateInfo const& other) const -> bool = default;
    };

    auto depth_attachment_create_info(math::uint2 extent, EFormat format) -> TextureCreateInfo;
    auto contain_all_resources(TextureSubresourceSet* subresources, TextureCreateInfo const* info) -> bool;
    auto adapt_to_texture(TextureSubresourceSet* subresources, TextureCreateInfo const* info, bool signle_mip_level) -> void;

    struct RHITexture: IPoolableResource
    {
        CNE_INTERFACE(RHITexture);
        using IPoolableResource::IPoolableResource;

        using Description = TextureCreateInfo;

        virtual auto description() -> Description const* = 0;
        // Require a texture subresource bindless index, EDescriptorType must be sampled_texture or storage_texture.
        // If usage not contains the type need, will get an invalid bindless index.
        virtual auto descriptor_handle(
            TextureSubresourceSet subresources = {},
            EDescriptorType type = EDescriptorType::sampled_texture
        ) -> math::uint2 = 0;
    };

    struct TextureBarrier final
    {
        RHITexture* texture{};

        uint32_t mip_level{0};
        uint32_t array_layer{0};
        bool contain_all_resource{false};

        EResourceStates src_state{EResourceStates::unknown};
        EResourceStates dst_state{EResourceStates::unknown};
        EPipelineStage src_stage{EPipelineStage::none};
        EPipelineStage dst_stage{EPipelineStage::none};

        auto operator <=> (TextureBarrier const& other) const -> bool = default;
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

    struct VertexInputState final
    {
        struct VertexAttribute
        {
            uint8_t location{0};
            uint8_t offset_bytes{0};
            EFormat format{};
        };

        struct VertexStream
        {
            uint8_t binding{0};
            EVertexInputRate input_rate{EVertexInputRate::vertex};
            uint16_t stride{0};
            std::vector<VertexAttribute> attributes{};

            auto add_attribute(uint8_t location, uint8_t offset, EFormat format) -> void
            {
                attributes.emplace_back(location, offset, format);
            }
        };
        std::vector<VertexStream> streams{};

        auto add_stream(uint16_t stride, EVertexInputRate input_rate) -> VertexStream*
        {
            return &streams.emplace_back((uint8_t) streams.size(), input_rate, stride);
        }
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

    struct BlendState final
    {
        bool enable_blend{false};

        EBlendOperation color_blend_op{EBlendOperation::add};
        EBlendFactor color_src_blend{EBlendFactor::one};
        EBlendFactor color_dst_blend{EBlendFactor::zero};
        EBlendOperation alpha_blend_op{EBlendOperation::add};
        EBlendFactor alpha_src_blend{EBlendFactor::one};
        EBlendFactor alpha_dst_blend{EBlendFactor::zero};
        EColorWriteMask color_write_mask{EColorWriteMask::rgba};

        auto operator <=> (BlendState const& other) const = default;
    };

    struct RasterizerState final
    {
        ERasterizerTopologyType topology{ERasterizerTopologyType::triangle_list};
        ERasterizerFillMode fill_mode{ERasterizerFillMode::solid};
        ERasterizerCullMode cull_mode{ERasterizerCullMode::none};
        ERasterizerFrontFace front_face{ERasterizerFrontFace::counter_clockwise};

        auto operator <=> (RasterizerState const& other) const = default;
    };

    struct DepthState final
    {
        bool enable_depth_test{true};
        bool enable_depth_write{true};

        ECompareOperation depth_compare{ECompareOperation::greater};
        float depth_bias{0.0f};

        auto operator <=> (DepthState const& other) const = default;
    };

    struct StencileState final
    {
        bool enable_stencil{false};

        ECompareOperation stencil_test{ECompareOperation::never};
        EStencilOperation stencil_fail_operation{EStencilOperation::keep};
        EStencilOperation depth_fail_operation{EStencilOperation::keep};
        EStencilOperation pass_operation{EStencilOperation::keep};

        auto operator <=> (StencileState const& other) const = default;
    };

    struct Attachment final
    {
        TextureHandle texture{};
        TextureSubresourceSet subresources{0, 1, 0, 1};
        ELoadOp load{ELoadOp::clear};
        EStoreOp store{EStoreOp::store};
        math::float4 clear_color{0.0f};
        float clear_depth{0.0f};
        uint8_t clear_stencil{0};

        explicit constexpr operator bool () noexcept
        {
            return (bool) texture;
        }

        auto operator == (Attachment const& other) const -> bool = default;
    };

    struct RenderTargetInfo final
    {
        math::int2 offset{};
        math::uint2 extent{};
        nonstd::inplace_vector<EFormat, k_max_render_targets> color_formats{};
        nonstd::inplace_vector<BlendState, k_max_render_targets> blend_states{};
        EFormat depth_stencil_format{EFormat::undefined};
        DepthState depth_state{};
        StencileState stencil_state{};
        uint32_t num_samples{1};

        auto operator == (RenderTargetInfo const& other) const -> bool = default;

        auto add_color_info(EFormat format, BlendState blend_state) -> void
        {
            color_formats.emplace_back(std::move(format));
            blend_states.emplace_back(std::move(blend_state));
        }
    };

    struct RenderTarget final
    {
        RenderTargetInfo info{};
        nonstd::inplace_vector<Attachment, k_max_render_targets> color_attachments{};
        Attachment depth_stencil_attachment{};

        auto operator == (RenderTarget const& other) const -> bool = default;
    };

    struct GraphicsPipelineCreateInfo final
    {
        ShaderModuleHandle vs{};
        ShaderModuleHandle fs{};

        // TODO: gs cs.

        RenderTargetInfo render_target_info{};
        ERasterizerTopologyType topology{ERasterizerTopologyType::triangle_list};
    };

    // PSO: Pipeline State Object
    struct RHIGraphicsPipeline: IResource
    {
        CNE_INTERFACE(RHIGraphicsPipeline);
        using IResource::IResource;
    };

    struct MeshPipelineCreateInfo final
    {
        ShaderModuleHandle as{};
        ShaderModuleHandle ms{};
        ShaderModuleHandle fs{};

        RenderTargetInfo render_target_info{};
        ERasterizerTopologyType topology{ERasterizerTopologyType::triangle_list};
    };

    struct RHIMeshPipeline: IResource
    {
        CNE_INTERFACE(RHIMeshPipeline);
        using IResource::IResource;
    };

    struct ComputePipelineCreateInfo final
    {
        ShaderModuleHandle compute_shader{};
        size_t push_constant_size{};
    };

    struct RHIComputePipeline: IResource
    {
        CNE_INTERFACE(RHIComputePipeline);
        using IResource::IResource;
    };

    struct RayTracingPipelineCreateInfo final
    {
        ShaderModuleHandle rgen{};
        ShaderModuleHandle rchit{};
        ShaderModuleHandle rmiss{};

        size_t max_recursion_depth{1};
        size_t push_constant_size{};
    };

    struct RHIRayTracingPipeline: IResource
    {
        CNE_INTERFACE(RHIRayTracingPipeline);
        using IResource::IResource;
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

        virtual auto backbuffer() -> TextureHandle = 0;
        virtual auto num_backbuffers() -> uint32_t = 0;
        virtual auto backbuffer_index() -> uint32_t = 0;
        virtual auto acquire_next_backbuffer() -> TextureHandle = 0;
        virtual auto present(uint64_t submission_time) -> void = 0;
        virtual auto enqueue_backbuffer_ready_wait_semaphore() -> void = 0;
        virtual auto enqueue_render_finish_signal_semaphore() -> void = 0;
    };

    struct RHITimerQuery: IResource
    {
        CNE_INTERFACE(RHITimerQuery);
        using IResource::IResource;
    };

    struct ViewportState final
    {
        nonstd::inplace_vector<Viewport, k_max_viewports> viewports{};
        nonstd::inplace_vector<Scissor, k_max_viewports> scissors{};

        explicit constexpr operator bool () noexcept
        {
            return !viewports.empty() || !scissors.empty();
        }

        auto operator <=> (ViewportState const& other) const = default;
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

    struct GraphicsState final
    {
        GraphicsPipelineHandle pipeline{};
        RenderTarget* render_target{};
        ViewportState viewport_state{};
        VertexInputState* vertex_input_state{};

        using VertexBufferBindings = nonstd::inplace_vector<VertexBufferBinding, k_max_vertex_attributes>;
        VertexBufferBindings vertex_buffer_bindings{};
        IndexBufferBinding index_buffer_binding{};

        BufferHandle indirect_buffer{};

        auto operator <=> (GraphicsState const& other) const = default;
    };

    struct MeshState final
    {
        MeshPipelineHandle pipeline{};
        RenderTarget* render_target{};
        ViewportState viewport_state{};

        BufferHandle indirect_buffer{};

        auto operator <=> (MeshState const& other) const = default;
    };

    struct ComputeState final
    {
        ComputePipelineHandle pipeline{};
        size_t push_constant_size{};

        BufferHandle indirect_buffer{};

        auto operator <=> (ComputeState const& other) const = default;
    };

    struct CommandListCreateInfo final
    {
        bool enable_immediate_submit{false};
        bool enable_automatic_barrier{true};
        EQueueType queue_type{EQueueType::graphics};

        auto operator <=> (CommandListCreateInfo const& other) const = default;
    };

    struct RHICommandList: IResource
    {
        CNE_INTERFACE(RHICommandList);
        using IResource::IResource;

        virtual auto start() -> void = 0;

        virtual auto finish() -> void = 0;

        virtual auto reset() -> void = 0;

        virtual auto clear_buffer_uint(BufferHandle buffer, uint32_t clear_value = 0u) -> void = 0;

        virtual auto clear_texture_float(TextureHandle texture, TextureSubresourceSet subresources, math::float4 clear_color) -> void = 0;

        virtual auto clear_depth_stencil(TextureHandle texture, TextureSubresourceSet subresources, std::optional<float> clear_depth, std::optional<uint8_t> clear_stencil) -> void = 0;

        virtual auto clear_texture_uint(TextureHandle texture, TextureSubresourceSet subresources, uint32_t clear_color) -> void = 0;

        virtual auto copy_buffer(BufferHandle src_buffer, size_t src_offset_byte, BufferHandle dst_buffer, size_t dst_offset_byte, size_t data_size_byte) -> void = 0;

        virtual auto copy_texture(TextureHandle src_texture, TextureSlice src_slice, TextureHandle dst_texture, TextureSlice dst_slice) -> void = 0;

        virtual auto write_buffer(BufferHandle buffer, std::span<std::byte> data, size_t offset_byte = 0) -> void = 0;

        virtual auto write_texture(TextureHandle texture, uint32_t level, uint32_t layer, TextureSliceDataView data) -> void = 0;

        virtual auto push_constants(void const* data, size_t size_bytes) -> void = 0;

        template <typename PushConstants>
        auto push_constants(PushConstants const& constants) -> void;

        virtual auto set_compute_state(ComputeState* state) -> void = 0;

        virtual auto dispatch(uint32_t group_count_x, uint32_t group_count_y = 1, uint32_t group_count_z = 1) -> void = 0;

        virtual auto dispatch_indirect(uint32_t offset = 0) -> void = 0;

        virtual auto set_graphics_state(GraphicsState* state) -> void = 0;

        virtual auto set_viewport_state(ViewportState* state) -> void = 0;

        virtual auto draw(DrawArguments* args) -> void = 0;

        virtual auto draw_indexed(DrawArguments* args) -> void = 0;

        virtual auto draw_indirect(uint32_t offset_bytes, uint32_t draw_count = 1) -> void = 0;

        virtual auto draw_indexed_indirect(uint32_t offset_bytes, uint32_t draw_count = 1) -> void = 0;

        virtual auto set_mesh_state(MeshState* state) -> void = 0;

        virtual auto dispatch_mesh(uint32_t group_count_x, uint32_t group_count_y = 1, uint32_t group_count_z = 1) -> void = 0;

        virtual auto dispatch_mesh_indirect(uint32_t offset = 0, uint32_t count = 1) -> void = 0;

        // TODO:raytracing.

        virtual auto push_command_label(std::string_view name, math::float4 color = math::float4{1.0f}) -> void = 0;

        virtual auto pop_command_label() -> void = 0;

        virtual auto begin_timestep(RHITimerQuery* query) -> void = 0;

        virtual auto end_timestep(RHITimerQuery* query) -> void = 0;

        virtual auto enbale_automatic_barriers(bool enable) -> void = 0;

        virtual auto begin_tracking_buffer(BufferHandle buffer, EResourceStates current_state, EPipelineStage current_stage) -> void = 0;

        virtual auto begin_tracking_texture(TextureHandle texture, TextureSubresourceSet subresources, EResourceStates current_state, EPipelineStage current_stage) -> void = 0;

        // If pipeline stage is not specified, it will be set based on the resource state.
        virtual auto set_buffer_state(BufferHandle buffer, EResourceStates dst_state, EPipelineStage dst_stage = EPipelineStage::none) -> void = 0;

        virtual auto set_texture_state(TextureHandle texture, TextureSubresourceSet subresources, EResourceStates dst_state, EPipelineStage dst_stage = EPipelineStage::none) -> void = 0;

        virtual auto lock_buffer_state(BufferHandle buffer, EResourceStates dst_state) -> void = 0;

        virtual auto lock_texture_state(TextureHandle texture, EResourceStates dst_state) -> void = 0;

        // Only can used when command list support immediate submit.
        virtual auto flush() -> void = 0;

        virtual auto buffer_state(BufferHandle buffer) -> EResourceStates = 0;

        virtual auto texture_state(TextureHandle texture, uint32_t level, uint32_t layer) -> EResourceStates = 0;

        // When transfer the ownership between different types of queues, need to specify the queue type.
        virtual auto commit_barriers(EQueueType src_queue = EQueueType::ignore, EQueueType dst_queue = EQueueType::ignore) -> void = 0;

        virtual auto wait_for_submit(EQueueType submit_queue_type, uint64_t submit_time, EPipelineStage wait_stage) -> void = 0;

        virtual auto device() -> IDevice* = 0;
    };
}

namespace cannele::inline graphics::rhi
{
    template <typename PushConstants>
    auto RHICommandList::push_constants(PushConstants const& constants) -> void
    {
        static_assert(std::is_object_v<PushConstants> && !std::is_pointer_v<PushConstants>, "PushConstants Type must be an object and not a pointer.");

        push_constants(&constants, sizeof(PushConstants));
    }
}
