#include "nanite_rendering.hpp"

#include <graphics/RHI/RHI.hpp>

namespace cannele::inline graphics::renderer
{
    inline namespace
    {
        REGISTER_SHADER_COMPOSITION(NaniteRenderMS, "nanite_render", "main_nanite_mesh_pass_ms", EShaderStage::mesh);
        REGISTER_SHADER_COMPOSITION(NaniteRenderFS, "nanite_render", "main_nanite_visibility_buffer_pass_fs", EShaderStage::fragment);
        REGISTER_SHADER_COMPOSITION(NaniteVisualizeFS, "nanite_visualization", "main_nanite_visualize_fs", EShaderStage::fragment);
    }
}
