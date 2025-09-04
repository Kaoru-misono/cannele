#pragma once

#include "common.slang.hpp"

namespace cannele
{
    // Must match the definition in nanite_instance_culling.slang
    struct InstanceCullingPushConstant
    {
        descriptor::BufferHandle frame_view_buffer;
        descriptor::BufferHandle cluster_group_count_buffer;
        descriptor::BufferHandle cluster_group_id_buffer;
        descriptor::BufferHandle scene_buffer;
    };

    struct NaniteVisualizationPushConstant
    {
        uint2 visibility_texture_size;
        descriptor::Sampler2DHandle visibility_texture;
        descriptor::BufferHandle meshlet_cmd_buffer;
        descriptor::BufferHandle scene_buffer;
        uint debug_type;
    };

    struct NaniteMeshRasterPushConstant
    {
        descriptor::BufferHandle frame_view_buffer;
        descriptor::BufferHandle meshlet_cmd_buffer;
        descriptor::BufferHandle scene_buffer;
        descriptor::BufferHandle debug_buffer;
    };

    struct IndirectDispatchCommandAssemblePushConstant
    {
        descriptor::BufferHandle count_buffer;
        descriptor::BufferHandle indirect_dispatch_command_buffer;
        uint32_t group_size;
        uint32_t offset;
    };

    struct NaniteClusterCullingPushConstant
    {
        descriptor::BufferHandle frame_view_buffer;
        descriptor::BufferHandle cluster_group_count_buffer;
        descriptor::BufferHandle cluster_group_id_buffer;
        descriptor::BufferHandle scene_buffer;
        descriptor::BufferHandle meshlet_count_buffer;
        descriptor::BufferHandle meshlet_cmd_buffer;
    };

    struct NaniteRenderIndirectDrawCommandAssemblePushConstant
    {
        descriptor::BufferHandle meshlet_count_buffer;
        descriptor::BufferHandle indirect_draw_command_buffer;
    };

    struct NaniteRenderFilterMeshletCmdsPushConstant
    {
        descriptor::BufferHandle camera_view;
        descriptor::BufferHandle gpu_scene;
        descriptor::BufferHandle meshlet_count_buffer;
        descriptor::BufferHandle passed_meshlet_count_buffer;
        descriptor::BufferHandle meshlet_cmd_buffer;
        descriptor::BufferHandle passed_meshlet_cmd_buffer;
    };

    struct NaniteDebugData
    {
        uint vertex_count;
        uint triangle_count;
        uint instance_id;
        uint meshlet_id;
        float4 position_local_space[3];
        uint3 triangle_indices;
        uint pad_0;
    };
}
