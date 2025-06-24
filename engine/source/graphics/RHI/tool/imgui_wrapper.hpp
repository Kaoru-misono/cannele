#pragma once

#include "../RHI.hpp"

#include <platform/window.hpp>

#include <imgui.h>

namespace cannele::inline graphics::rhi
{
    // A Simple Imgui Wrapper.
    struct ImGuiWrapper final
    {
        ImFont* imgui_current_font{};
        SamplerHandle font_sampler{};
        TextureHandle font_texture{};
        GraphicsPipelineHandle imgui_pipeline{};
        BufferHandle imgui_vertex_buffer{};
        BufferHandle imgui_index_buffer{};
        VertexInputState imgui_vertex_input_state{};

        ImGuiWrapper() = default;
        ImGuiWrapper(IDevice* device, platform::Window* window);
        ~ImGuiWrapper();

        auto new_frame() -> void;

        auto render(RHICommandList* cmd_list, TextureHandle texture) -> void;
    };

    using ImGuiWrapperHandle = RefCountPtr<ImGuiWrapper>;
}
