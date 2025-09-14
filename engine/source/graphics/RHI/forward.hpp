#pragma once

#include <core/idiom.hpp>
#include <core/log_system.hpp>
#include <math/type.hpp>

#include <string>
#include <memory>
#include <mdspan>

namespace cannele::inline graphics::rhi
{
    struct IDevice;
    struct IResource;
    struct IPoolableResource;
    struct RHIBuffer;
    struct RHITexture;
    struct RHISampler;
    struct RHIShaderModule;
    struct RHIGraphicsPipeline;
    struct RHIMeshPipeline;
    struct RHIComputePipeline;
    struct RHIRayTracingPipeline;
    struct RHISwapchain;
    struct RHITimerQuery;
    struct RHICommandList;

    using BufferHandle           = std::shared_ptr<RHIBuffer>;
    using TextureHandle          = std::shared_ptr<RHITexture>;
    using SamplerHandle          = std::shared_ptr<RHISampler>;
    using ShaderModuleHandle     = std::shared_ptr<RHIShaderModule>;
    using GraphicsPipelineHandle = std::shared_ptr<RHIGraphicsPipeline>;
    using MeshPipelineHandle     = std::shared_ptr<RHIMeshPipeline>;
    using ComputePipelineHandle  = std::shared_ptr<RHIComputePipeline>;
    using SwapchainHandle        = std::shared_ptr<RHISwapchain>;
    using TimerQueryHandle       = std::shared_ptr<RHITimerQuery>;
    using CommandListHandle      = std::shared_ptr<RHICommandList>;

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
        std::weak_ptr<IDevice> device{};
        std::atomic<std::shared_ptr<IDevice>> reference{};

        DeviceChild(IDevice* device);
        virtual ~DeviceChild();

        template <typename T = IDevice>
        auto get_device() -> T*
        {
            if (auto cached = reference.load(std::memory_order_acquire)) {
                return (T*) cached.get();
            }

            auto locked = device.lock();
            if (locked) {
                reference.store(locked, std::memory_order_release);
            } else {
                CNE_ERROR("Device lost.");
            }

            return (T*) locked.get();
        }

        auto invalidate_reference() -> void
        {
            reference.store(nullptr, std::memory_order_release);
        }
    };

    struct IResource: DeviceChild, std::enable_shared_from_this<IResource>
    {
        CNE_INTERFACE(IResource);

        std::string name{"Unknown"};

        IResource(IDevice* device)
            : DeviceChild(device)
        {}
    };

    struct BufferRange final
    {
        static constexpr auto max_size = std::numeric_limits<size_t>::max();

        size_t offset_bytes{};
        size_t size_bytes{max_size};

        BufferRange() = default;
        BufferRange(size_t offset, size_t size)
            : offset_bytes(offset)
            , size_bytes(size)
        {}

        auto operator <=> (BufferRange const& other) const = default;
    };

    // Different from BufferRange, the default TextureSubresourceSet is the level 0 and layer 0 slice of the given texture.
    struct TextureSubresourceSet final
    {
        uint32_t base_mip_level{0};
        uint32_t num_mip_levels{~0u};
        uint32_t base_array_layer{0};
        uint32_t num_array_layers{~0u};

        auto operator <=> (TextureSubresourceSet const& other) const = default;
    };

    static constexpr auto k_all_subresources = TextureSubresourceSet{};

    struct TextureSlice final
    {
        // Origin and extent describe a sub-region of the current slice.
        math::uint3 origin{};
        math::uint3 extent{};

        uint32_t level{0};
        uint32_t layer{0};

        auto operator == (TextureSlice const& other) const -> bool = default;
    };

    using TextureSliceDataView = std::mdspan<std::byte, std::extents<int, std::dynamic_extent, std::dynamic_extent, std::dynamic_extent>>;

    struct Viewport final
    {
        float x{};
        float y{};
        float width{};
        float height{};
        float min_depth{1.0f};
        float max_depth{0.0f};

        auto operator <=> (Viewport const& other) const = default;
    };

    struct Scissor final
    {
        int32_t x{};
        int32_t y{};
        uint32_t width{};
        uint32_t height{};

        auto operator <=> (Scissor const& other) const = default;
    };

    struct DrawArguments final
    {
        uint32_t num_vertices{};
        uint32_t num_instances{1};
        uint32_t first_vertex{};
        uint32_t first_instance{};
        uint32_t first_index{}; // For indexed draw.

        auto operator <=> (DrawArguments const& other) const = default;
    };
}
