
import bindless;

struct InstanceCullingPushConstant
{
    uint2 test_buffer;
};
[[vk::push_constant]] InstanceCullingPushConstant constant;

[[vk::binding(0, 0)]]
RWStructuredBuffer<uint> g_buffer[];

[shader("compute")]
[numthreads(1, 1, 1)]
void main_test_cs(uint work_group_id: SV_GroupID, uint local_thread_id: SV_GroupIndex)
{
    g_buffer[constant.test_buffer.x][NonUniformResourceIndex(0)] = 0;
}

[shader("vertex")]
void main_test_vs(uint vertex_id: SV_VertexID, out float4 position: SV_Position)
{
    float2 uv = float2((vertex_id & 2), (vertex_id & 1) << 1);
    position = float4(float2(uv * float2(2, -2) + float2(-1, 1)), 0.0, 1.0);
}


[shader("fragment")]
void main_test_fs(float2 fragment_coord: SV_Position, out float4 color: SV_Target)
{
    RWStructuredBuffer<uint> buffer = g_buffer[constant.test_buffer.x];
    buffer[(int) (fragment_coord.x + fragment_coord.y * 900)] = uint(fragment_coord.x + fragment_coord.y);
    color = float4(buffer.Load((int) (fragment_coord.x + fragment_coord.y * 900)) / (900 + 600), 0.0, 0.0, 1.0);
    // color = float4(1.0, 0.0, 0.0, 1.0);
}
