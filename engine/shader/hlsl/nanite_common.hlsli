struct TriangleMiscInfo
{
    GLTFMeshlet meshlet;
    uint triangle_index_id;

    float3  position[3];
    float4  position_clip_space[3];

    // Used for motion vector.
    float3  position_clip_space_no_jitter[3];
    float3  position_clip_space_no_jitter_last_frame[3];

    float3 tangent[3];
    float3 bitangent[3];
    float3 normal[3];
    float2 uv[3];
};
