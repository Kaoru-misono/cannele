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
