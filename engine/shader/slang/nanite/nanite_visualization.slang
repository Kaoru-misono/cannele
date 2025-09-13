#include "nanite_common.slang"

import camera;
import hash;
import scene;
import bindless;

struct FullScreenVSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

static const float3 lod_debug_color[k_nanite_max_lod_count] = {
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
void main_nanite_visualize_fs(
    in FullScreenVSOutput input,
    out float4 color: SV_Target,
    uniform uint2 visibility_texture_size,
    uniform Sampler2DHandle<uint> visibility_texture,
    uniform StructuredBufferHandle<uint3> meshlet_cmds,
    uniform StructuredBufferHandle<GpuScene> gpu_scene,
    uniform uint debug_type
)
{
    GpuScene scene = gpu_scene[0];

    uint pack_id = visibility_texture.SampleLevel(input.uv, 0);

    uint triangle_id = {};
    uint index_in_cmd_buffer = {};
    decode_triangle_and_array_index(pack_id, triangle_id, index_in_cmd_buffer);

    uint3 draw_cmd = meshlet_cmds[index_in_cmd_buffer];
    if (draw_cmd.z != index_in_cmd_buffer) {
        color = float4(1.0, 0.0, 1.0, 1.0);
        return;
    }

    uint instance_id = draw_cmd.x;
    uint meshlet_id = draw_cmd.y;

    // InstanceData instance_info = scene.instance_datas[instance_id];
    // GltfMaterialData material_info = scene.gltf_primitive_materials[instance_info.material_data_index];

    if (debug_type == 0) {
        color = float4(simple_hash_color(meshlet_id), 1.0);
    }
    else if (debug_type == 1) {
        color = float4(simple_hash_color(triangle_id), 1.0);
    }
}

