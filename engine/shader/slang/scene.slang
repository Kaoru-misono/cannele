import bindless;

struct InstanceData
{
    float4x4 matrix_local_to_world;
    float4x4 matrix_world_to_local;

    float4x4 matrix_local_to_world_last_frame;

    uint primitive_detail_index;
    uint material_data_index;
};

struct GltfPrimitiveInfo
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

    uint lod_0_meshlet_count;
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

struct MeshletGroup
{
    float3 cluster_position_center;
    float parent_error;

    float3 parent_position_center;
    float error;

    uint meshlet_offset;
    uint meshlet_count;
};


struct GltfPrimitiveDataBuffers
{
    StructuredBufferHandle<Meshlet> meshlets;
    ByteAddressBufferHandle positions;
    StructuredBufferHandle<float3> normals;
    StructuredBufferHandle<float2> texcoords_0;

    StructuredBufferHandle<float3> tangents;
    StructuredBufferHandle<float2> texcoords_1;
    StructuredBufferHandle<float3> colors;
    StructuredBufferHandle<float3> smooth_normals;

    StructuredBufferHandle<uint> meshlet_datas;
    StructuredBufferHandle<float> bvh_nodes;
    StructuredBufferHandle<MeshletGroup> meshlet_groups;
    StructuredBufferHandle<uint> meshlet_group_indices;

    StructuredBufferHandle<uint> lod_0_indices;
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

    Sampler2DHandle base_color_texture;
    Sampler2DHandle emissive_texture;
    Sampler2DHandle normal_texture;
    Sampler2DHandle metallic_roughness_texture;

    float normal_factor_scale;
    float occlusion_texture_strength;
    uint material_type;
};

struct GpuScene
{
    StructuredBufferHandle<InstanceData> instance_datas;
    StructuredBufferHandle<GltfPrimitiveInfo> gltf_primitive_details;
    StructuredBufferHandle<GltfPrimitiveDataBuffers> gltf_primitive_datas;
    StructuredBufferHandle<GltfMaterialData> gltf_primitive_materials;

    uint gltf_object_count;
    uint pad;
};
