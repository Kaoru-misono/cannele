#pragma once

#include <core/resource/asset.hpp>
#include <graphics/RHI/device.hpp>
#include <graphics//renderer/renderer.hpp>

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

        Engine(EngineCreateInfo* info);
        ~Engine();

        auto run() -> void;
    };
}
