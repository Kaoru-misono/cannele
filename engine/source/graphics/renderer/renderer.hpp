#pragma once

#include "../RHI/RHI.hpp"
#include "../RHI/tool/imgui_wrapper.hpp"

#include <core/idiom.hpp>

namespace cannele::inline graphics::renderer
{
    struct Renderer
    {
        CNE_INTERFACE(Renderer);

        virtual auto render() -> void = 0;
    };

    struct RendererCreateInfo final
    {
        rhi::IDevice* device{};
        rhi::RHISwapchain* swapchain{};
        rhi::ImGuiWrapper* imgui{};
    };

    struct DeferredRenderer final: Renderer
    {
        rhi::IDevice* device{};
        rhi::RHISwapchain* swapchain{};
        rhi::ImGuiWrapper* imgui_wrapper{};
        rhi::CommandListHandle command_list{};
        std::vector<rhi::TimerQueryHandle> timer_querys{};
        uint32_t frame_count{0};

        DeferredRenderer(RendererCreateInfo* info);
        ~DeferredRenderer();

        auto render() -> void override;
    };
}
