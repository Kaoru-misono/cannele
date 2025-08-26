#pragma once

#include "render_context.hpp"

namespace cannele::inline graphics::renderer
{
    auto instance_culling(rhi::CommandListHandle command_list, RenderContext* context) -> std::pair<rhi::BufferHandle, rhi::BufferHandle>;
}
