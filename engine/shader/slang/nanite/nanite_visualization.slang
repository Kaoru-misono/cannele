#include "nanite_common.slang"

import camera;
import hash;
import scene;
import bindless;

struct NaniteVisualizationPushConstant
{
    uint2 visibility_texture_size;
    Sampler2DHandle<uint> visibility_texture;
    StructuredBufferHandle<FrameViewData> camera_views;
    StructuredBufferHandle<uint3> meshlet_cmds;
    StructuredBufferHandle<GpuScene> scene;

    uint debug_type;
};
[[vk::push_constant]] NaniteVisualizationPushConstant constant;

struct FullScreenVSToFS
{
    float4 position: SV_Position;
    float2 uv: TEXCOORD0;
};

static const float3 lod_debug_color[NANITE_MAX_LOD_COUNT] = {
    float3(1.0, 0.0, 0.0),
    float3(0.7, 0.3, 0.0),
    float3(0.4, 0.6, 0.0),
    float3(0.1, 0.9, 0.0),
    float3(0.0, 1.0, 0.2),
    float3(0.0, 0.5, 0.6),
    float3(0.0, 0.1, 0.8),
    float3(0.0, 0.0, 1.0),
    float3(0.1, 0.1, 0.8),
    float3(0.2, 0.2, 0.6),
    float3(0.0, 0.4, 0.7),
    float3(0.2, 0.6, 0.3),
};

[shader("fragment")]
void main_nanite_visualize_fs(in FullScreenVSToFS input, out float4 color: SV_Target)
{
    FrameViewData view = constant.camera_views[0];
    GpuScene scene = constant.scene[0];

    uint pack_id = constant.visibility_texture.SampleLevel(input.uv, 0);

    if (pack_id == 0) {
        color = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    uint triangle_id;
    uint instance_id;
    decode_triangle_and_instance_id(pack_id, triangle_id, instance_id);

    uint3 draw_cmd = constant.meshlet_cmds[instance_id];

    uint object_id = draw_cmd.x;
    uint meshlet_id = draw_cmd.y;

    InstanceData object_info = scene.gltf_objects[object_id];
    GltfMaterialData material_info = scene.gltf_primitive_materials[object_info.material_data_index];

    color = float4(simple_hash_color(meshlet_id), 1.0);
}
