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
        descriptor::BufferHandle frame_view_buffer;
        descriptor::BufferHandle meshlet_cmd_buffer;
        descriptor::BufferHandle scene_buffer;

        uint debug_type;
    };

    struct InstancePushConstant
    {
        descriptor::BufferHandle frame_view_buffer;
        descriptor::BufferHandle meshlet_cmd_buffer;
        descriptor::BufferHandle scene_buffer;
    };
}
