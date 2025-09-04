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

        std::vector<InstanceData> instances_data{};
        std::vector<GltfPrimitiveInfo> primitive_infos_data{};
        std::vector<GltfPrimitiveDataBuffers> primitive_data_buffer_data{};

        rhi::TextureHandle backbuffer{};
        rhi::TextureHandle depth_texture{};
        rhi::TextureHandle visibility_texture{};

        rhi::BufferHandle frame_view_buffer{};
        rhi::BufferHandle gpu_scene_buffer{};
        rhi::BufferHandle gltf_instance_info_buffer{};
        rhi::BufferHandle gltf_primitive_detail_buffer{};
        rhi::BufferHandle gltf_primitive_data_buffer{};
        rhi::BufferHandle gltf_material_buffer{};

        // For Nanite.
        rhi::BufferHandle meshlet_count_buffer{};
        rhi::BufferHandle meshlet_cmd_buffer{};
        // rhi::BufferHandle meshlet_filtered_cmd_buffer{};
        rhi::BufferHandle meshlet_group_count_buffer{};
        rhi::BufferHandle meshlet_group_id_buffer{};
    };
}
