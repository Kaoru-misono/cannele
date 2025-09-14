#pragma once

#include "render_context.hpp"
#include "../RHI/device.hpp"
#include "../RHI/tool/imgui_wrapper.hpp"

#include <core/idiom.hpp>
#include <scene/camera.hpp>
#include <view_data.slang.hpp>

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
        std::vector<rhi::TimerQueryHandle> timer_querys{};

        std::unique_ptr<scene::Camera> camera{};
        std::unique_ptr<RenderContext> context{};

        FrameViewData per_frame_view_data{};

        uint32_t frame_count{0};

        DeferredRenderer(RendererCreateInfo* info);
        ~DeferredRenderer();

        auto render() -> void override;
    };

    struct TestComputeRenderer final: Renderer
    {
        rhi::IDevice* device{};
        rhi::RHISwapchain* swapchain{};
        rhi::ImGuiWrapper* imgui_wrapper{};
        rhi::ShaderModuleHandle shader_module{};
        rhi::ShaderProgramHandle shader_program{};
        rhi::ShaderProgramHandle compute_program{};

        uint32_t frame_count{0};

        TestComputeRenderer(RendererCreateInfo* info);
        ~TestComputeRenderer();

        auto render() -> void override;
    };
}
