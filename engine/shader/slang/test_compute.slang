
[vk::binding(0, 0)]
__DynamicResource<__DynamicResourceKind.General> resourceHandles[];

[vk::binding(0, 1)]
__DynamicResource<__DynamicResourceKind.Sampler> samplerHandles[];

export T getDescriptorFromHandle<T>(DescriptorHandle<T> handle) where T : IOpaqueDescriptor
{
    __target_switch
    {
    case spirv:
        if (T.kind == DescriptorKind.Sampler) {
            return samplerHandles[((uint2)handle).x].asOpaqueDescriptor<T>();
        }
        else {
            return resourceHandles[((uint2)handle).x].asOpaqueDescriptor<T>();
        }
    default:
        return defaultGetDescriptorFromHandle(handle);
    }
}

struct PushConstant
{
    RWByteAddressBuffer.Handle test_buffer;
};
[[vk::push_constant]] PushConstant constant;

[[vk::binding(0, 0)]]
RWByteAddressBuffer g_buffer[];

[shader("compute")]
[numthreads(1, 1, 1)]
void main_test_cs(uint work_group_id: SV_GroupID, uint local_thread_id: SV_GroupIndex)
{
    // constant.test_buffer.Store<uint3>(0, uint3(1, 0, 0));
    uint2 handle = uint2(constant.test_buffer);
    g_buffer[handle.x].Store<uint3>(0, uint3(1, 0, 0));
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
    uint2 handle = uint2(constant.test_buffer);
    // color = float4(constant.test_buffer.Load<uint3>(0), 1.0);
    color = float4(g_buffer[handle.x].Load<uint3>(0), 1.0);
}
