#pragma once

#include <graphics/RHI/RHI_resource.hpp>
#include <scene/resource/gltf_asset.hpp>
#include <scene.slang.hpp>

namespace cannele::inline graphics::renderer
{
    struct RenderContext final
    {
        scene::resource::GLTFAsset* asset{};
        GpuScene scene{};

        rhi::BufferHandle frame_view_buffer{};
        rhi::BufferHandle gpu_scene_buffer{};
        rhi::BufferHandle gltf_instance_info_buffer{};
        rhi::BufferHandle gltf_primitive_detail_buffer{};
        rhi::BufferHandle gltf_primitive_data_buffer{};
        rhi::BufferHandle gltf_material_buffer{};
    };
}
