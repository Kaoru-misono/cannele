static const uint k_nanite_max_meshlet_vertices = 255;
static const uint k_nanite_max_meshlet_triangles = 128;
static const uint k_nanite_max_lod_count = 12;
static const uint k_max_instance_id_count = 0xffffff;

static const uint k_cluster_group_merge_max_count = 4;

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

uint encode_triangle_and_instance_id(uint triangle_id, uint instance_id)
{
    uint next_instnace_id = instance_id + 1;
    return (((next_instnace_id & 0xffffff) << 8) | (triangle_id & 0xff));
}

void decode_triangle_and_instance_id(uint pack, out uint triangle_id, out uint instance_id)
{
    triangle_id = (pack & 0xff);
    instance_id = ((pack >> 8) & 0xffffff) - 1;
}

uint unpack_vertex_count(uint packed_value)
{
    return packed_value & 0xff;
}

uint unpack_triangle_count(uint packed_value)
{
    return ((packed_value >> 8) & 0xff);
}
