#pragma once

#include "render_context.hpp"

namespace cannele::inline graphics::renderer
{
    auto instance_culling(rhi::CommandEncoderHandle encoder, RenderContext* context) -> std::pair<rhi::BufferHandle, rhi::BufferHandle>;

    auto nanite_render_pass_0(rhi::CommandEncoderHandle encoder, RenderContext* context) -> void;

    auto nanite_visualize(rhi::CommandEncoderHandle encoder, RenderContext* context) -> void;

    auto nanite_shading(rhi::CommandEncoderHandle encoder, RenderContext* context) -> void;
}
