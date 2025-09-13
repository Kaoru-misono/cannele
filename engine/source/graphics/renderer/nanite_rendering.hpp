#pragma once

#include "render_context.hpp"

namespace cannele::inline graphics::renderer
{
    auto instance_culling(rhi::CommandListHandle command_list, RenderContext* context) -> std::pair<rhi::BufferHandle, rhi::BufferHandle>;

    auto nanite_render_pass_0(rhi::CommandListHandle command_list, RenderContext* context) -> void;

    auto nanite_visualize(rhi::CommandListHandle command_list, RenderContext* context) -> void;

    auto nanite_shading(rhi::CommandListHandle command_list, RenderContext* context) -> void;
}
