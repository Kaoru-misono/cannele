#define NANITE_MAX_MESHLET_VERTICES 255
#define NANITE_MAX_MESHLET_TRIANGLES 128
#define NANITE_MAX_LOD_COUNT 12
#define MAX_INSTANCE_ID_COUNT 0xffffff

// struct TriangleMiscInfo
// {
//     GLTFMeshlet meshlet;
//     uint triangle_index_id;
//
//     float3  position[3];
//     float4  position_clip_space[3];
//
//     // Used for motion vector.
//     float3  position_clip_space_no_jitter[3];
//     float3  position_clip_space_no_jitter_last_frame[3];
//
//     float3 tangent[3];
//     float3 bitangent[3];
//     float3 normal[3];
//     float2 uv[3];
// };

struct GpuScene
{
    StructuredBuffer<GltfPrimitiveBuffer>.Handle gltf_primitive_detail_buffer;
    StructuredBuffer<GltfPrimitiveDataBuffers>.Handle gltf_primitive_data_buffer;
    StructuredBuffer<GltfMaterialData>.Handle gltf_primitive_material_buffer;

    StructuredBuffer<GpuObjectGltfPrimitive>.Handle gltf_object_buffer;
    uint gltf_object_count;
    uint pad;
};

struct GpuObjectGltfPrimitive
{
    float4x4 matrix_local_to_world;
    float4x4 matrix_world_to_local;

    float4x4 matrix_local_to_world_last_frame;

    uint primitive_detail_index;
    uint material_data_index;
};

struct GltfPrimitiveBuffer
{
    float3 position_min;
    uint vertex_offset;

    float3 position_max;
    uint vertex_count;

    float3 position_average;
    uint pad;

    uint meshlet_offset;
    uint color_0_offset;
    uint smooth_normal_offset;
    uint texcoord_1_offset;

    uint bvh_node_offset;
    uint meshlet_group_offset;
    uint meshlet_group_indices_offset;
    uint meshlet_group_count;

    uint lod_0_indices_offset;
    uint lod_0_indices_count;

    uint data_buffer_index;
};

struct Meshlet
{
    float3 position_min;
    uint data_offset;

    float3 position_max;
    uint vertex_triangle_count;

    float3 cone_axis;
    float cut_off;

    float3 cone_apex;
    uint lod;
};

struct GltfPrimitiveDataBuffers
{
    StructuredBuffer<Meshlet>.Handle meshlet_buffer;
    StructuredBuffer<float3>.Handle position_buffer;
    StructuredBuffer<float3>.Handle normal_buffer;
    StructuredBuffer<float2>.Handle texcoord_0_buffer;

    StructuredBuffer<float3>.Handle tangent_buffer;
    StructuredBuffer<float2>.Handle texcoord_1_buffer;
    StructuredBuffer<float3>.Handle color_buffer;
    StructuredBuffer<float3>.Handle smooth_normal_buffer;

    StructuredBuffer<uint>.Handle meshlet_data_buffer;
    StructuredBuffer<float>.Handle bvh_node_buffer;

};

struct GltfMaterialData
{
    uint alpha_mode;
    float alpha_cutoff;
    uint is_two_side;

    float4 base_color_factor;

    float3 emissive_factor;

    float metallic_factor;
    float roughness_factor;

    Sampler2D.Handle base_color_texture;
    Sampler2D.Handle emissive_texture;
    Sampler2D.Handle normal_texture;
    Sampler2D.Handle metallic_roughness_texture;

    float normal_factor_scale;
    float occlusion_texture_strength;
    uint material_type;
};

uint encode_triangle_and_instance_id(uint triangle_id, uint instance_id)
{
    uint next_instnace_id = instance_id + 1;
    return (((next_instnace_id & MAX_INSTANCE_ID_COUNT) << 8) | (triangle_id & 0xff));
}

void decode_triangle_and_instance_id(uint pack, out uint triangle_id, uint instance_id)
{
    triangle_id = (pack & 0xff);
    instance_id = ((pack >> 8) & MAX_INSTANCE_ID_COUNT) - 1;
}
