#pragma once

#include "definitions.hpp"

#include <core/idiom.hpp>
#include <core/log_system.hpp>
#include <core/breakable_reference.hpp>
#include <math/type.hpp>

#include <memory>
#include <mdspan>

namespace cannele::inline graphics::rhi
{
    struct IDevice;
    struct IResource;
    struct IPoolableResource;
    struct RHIBuffer;
    struct RHITexture;
    struct RHITextureView;
    struct RHISampler;
    struct RHIShaderModule;
    struct RHIGraphicsPipeline;
    struct RHIComputePipeline;
    struct RHIRayTracingPipeline;
    struct RHISwapchain;
    struct RHITimerQuery;
    struct CommandList;
    struct RHIShaderProgram;
    struct ShaderObjectLayout;

    using BufferHandle           = std::shared_ptr<RHIBuffer>;
    using TextureHandle          = std::shared_ptr<RHITexture>;
    using TextureViewHandle      = std::shared_ptr<RHITextureView>;
    using SamplerHandle          = std::shared_ptr<RHISampler>;
    using ShaderModuleHandle     = std::shared_ptr<RHIShaderModule>;
    using GraphicsPipelineHandle = std::shared_ptr<RHIGraphicsPipeline>;
    using ComputePipelineHandle  = std::shared_ptr<RHIComputePipeline>;
    using SwapchainHandle        = std::shared_ptr<RHISwapchain>;
    using TimerQueryHandle       = std::shared_ptr<RHITimerQuery>;
    using ShaderProgramHandle    = std::shared_ptr<RHIShaderProgram>;

    template <typename T>
    using ResourceOwned = std::unique_ptr<T>;

    static constexpr auto k_invalid_bindless_index = ~0u;
    static constexpr auto k_max_viewports = 4;
    static constexpr auto k_max_render_targets = 8;
    static constexpr auto k_max_vertex_attributes = 8;
    static constexpr auto k_invalid_time = std::numeric_limits<uint64_t>::max();

    struct NativeObject final
    {
        void* pointer;

        NativeObject(void* pointer) : pointer(pointer) {}

        template <typename T>
        operator T* () const
        {
            return (T*) pointer;
        }
    };

    // All external used resource should inherit this to keep a reference to device.
    struct DeviceChild
    {
    private:

        BreakableReference<IDevice> device{};

    public:

        DeviceChild(IDevice* device);
        virtual ~DeviceChild() = default;

        template <typename T = IDevice>
        auto get_device() -> T*
        {
            return (T*) device.get();
        }
    };

    struct IResource: DeviceChild, std::enable_shared_from_this<IResource>
    {
        CNE_INTERFACE(IResource);

        std::string name{"Unknown"};

        IResource(IDevice* device);
    };

    struct BufferRange final
    {
        static constexpr auto max_size = std::numeric_limits<size_t>::max();

        size_t offset{};
        size_t size{max_size};

        BufferRange() = default;
        BufferRange(size_t offset, size_t size)
            : offset(offset)
            , size(size)
        {}

        auto operator <=> (BufferRange const& other) const = default;
    };

    using TextureSliceDataView = std::mdspan<std::byte const, std::extents<int, std::dynamic_extent, std::dynamic_extent, std::dynamic_extent>>;

    // Different from BufferRange, the default TextureSubresourceSet is the level 0 and layer 0 slice of the given texture.
    struct TextureSubresourceRange final
    {
        uint32_t mip_level{0};
        uint32_t mip_count{1};
        uint32_t layer{0};
        uint32_t layer_count{1};

        auto operator <=> (TextureSubresourceRange const& other) const = default;
    };

    static constexpr auto k_all_subresources = TextureSubresourceRange{0, 0xffffffff, 0, 0xffffffff};

    struct TextureSlice final
    {
        // Origin and extent describe a sub-region of the current slice.
        math::uint3 origin{};
        math::uint3 extent{};

        uint32_t mip_level{0};
        uint32_t layer{0};

        auto operator == (TextureSlice const& other) const -> bool = default;
    };

    struct ColorAttachment final
    {
        RHITextureView* view{};
        ELoadOp load{ELoadOp::clear};
        EStoreOp store{EStoreOp::store};
        math::float4 clear_color{0.0f};

        explicit constexpr operator bool () noexcept
        {
            return (bool) view;
        }

        auto operator == (ColorAttachment const& other) const -> bool = default;
    };

    struct DepthStencilAttachment final
    {
        RHITextureView* view{};
        ELoadOp depth_load{ELoadOp::clear};
        EStoreOp depth_store{EStoreOp::store};
        ELoadOp stencil_load{ELoadOp::clear};
        EStoreOp stencil_store{EStoreOp::store};
        float clear_depth{0.0f};
        uint8_t clear_stencil{0};
        bool depth_read_only{false};
        bool stencil_read_only{false};

        explicit constexpr operator bool () noexcept
        {
            return (bool) view;
        }

        auto operator == (DepthStencilAttachment const& other) const -> bool = default;
    };

    struct BufferView
    {
        RHIBuffer* buffer{};
        size_t offset{};

        BufferView() = default;
        BufferView(BufferHandle buffer, size_t offset)
            : buffer(buffer.get())
            , offset(offset)
        {}

        auto operator <=> (BufferView const& other) const = default;
    };

    struct GraphicsState
    {
        std::span<Viewport> viewports{};
        std::span<Scissor> scissors{};
        std::span<BufferView> vertex_buffers{};
        BufferView index_buffer{};
        EIndexType index_type{EIndexType::uint16};
        VertexInputState* vertex_input_state{};
        std::span<BlendState> blend_states{};
        DepthStencilState depth_stencil_state{};
    };
}
