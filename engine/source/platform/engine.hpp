#pragma once

#include <graphics/RHI/RHI.hpp>
#include <graphics//renderer/renderer.hpp>

namespace cannele::inline platform
{
    struct EngineCreateInfo final
    {
        math::uint2 initial_window_size{};
    };

    struct Engine final
    {
    private:

        std::unique_ptr<Window> window{};
        rhi::DeviceHandle device{};
        rhi::SwapchainHandle swapchain{};
        rhi::ImGuiWrapperHandle imgui{};
        std::unique_ptr<renderer::Renderer> renderer{};

    public:

        Engine(EngineCreateInfo* info);
        ~Engine();

        auto run() -> void;
    };
}
