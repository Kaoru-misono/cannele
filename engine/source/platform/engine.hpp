#pragma once

#include <core/resource/asset.hpp>
#include <graphics/RHI/RHI.hpp>
#include <graphics//renderer/renderer.hpp>
#include <scene/resource/gltf_asset.hpp>

namespace cannele::inline platform
{
    struct EngineCreateInfo final
    {
        math::uint2 initial_window_size{};
    };

    struct Engine final: ThreadExclusive<Engine>
    {
        std::unique_ptr<Window> window{};
        rhi::DeviceHandle device{};
        rhi::SwapchainHandle swapchain{};
        rhi::ImGuiWrapperHandle imgui{};
        std::unique_ptr<core::resource::AssetManager> asset_manager{};
        std::unique_ptr<renderer::Renderer> renderer{};

        scene::resource::GLTFAsset* asset{};

        Engine(EngineCreateInfo* info);
        ~Engine();

        auto run() -> void;
    };
}
