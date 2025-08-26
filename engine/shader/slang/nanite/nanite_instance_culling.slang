#include "nanite_common.slang"

import camera;
import bindless;

struct InstanceCullingPushConstant
{
    StructuredBuffer<PerFrameCameraView>.Handle camera_view;
    RWStructuredBuffer<uint>.Handle cluster_group_count_buffer;
    RWStructuredBuffer<uint2>.Handle cluster_group_id_buffer;
    StructuredBuffer<GpuScene>.Handle scene;
};
[[vk::push_constant]] InstanceCullingPushConstant constant;

groupshared uint shared_store_base_id;
groupshared uint shared_meshlet_group_count;

[shader("compute")]
[numthreads(64, 1, 1)]
void main_instance_culling_cs(uint work_group_id: SV_GroupID, uint local_thread_id: SV_GroupIndex)
{
    PerFrameCameraView view = constant.camera_view[0];
    GpuScene scene = constant.scene[0];

    bool is_visible = true;
    bool is_first_thread = (local_thread_id == 0);

    uint bvh_node_count = 0;
    uint meshlet_group_count = 0;
    uint store_base_id = 0;
    if (is_first_thread) {
        GpuObjectGltfPrimitive object_info = scene.gltf_object_buffer[work_group_id];
        GltfPrimitiveBuffer primitive_info = scene.gltf_primitive_detail_buffer[object_info.primitive_detail_index];

        float4x4 matrix_local_to_world = object_info.matrix_local_to_world;
        float4x4 matrix_local_to_clip = mul(view.world_to_clip_matrix, matrix_local_to_world);

        float3 position_min = primitive_info.position_min;
        float3 position_max = primitive_info.position_max;
        float3 position_center = (position_min + position_max) * 0.5;
        float3 extent = position_max - position_center;

        // TODO: Frustum visible culling.

        if (is_visible) {
            GltfPrimitiveDataBuffers primitive_data_info = scene.gltf_primitive_data_buffer[primitive_info.data_buffer_index];
            // TODO: BVH node.
            meshlet_group_count += primitive_info.meshlet_group_count;
            RWStructuredBuffer<uint> cluster_group_count_buffer = constant.cluster_group_count_buffer;
            InterlockedAdd(cluster_group_count_buffer[0], meshlet_group_count, store_base_id);
        }
    }

    if (is_first_thread) {
        shared_store_base_id = store_base_id;
        shared_meshlet_group_count = meshlet_group_count;
    }
    GroupMemoryBarrierWithGroupSync();

    meshlet_group_count = shared_meshlet_group_count;
    store_base_id = shared_store_base_id;

    if (is_visible) {
        for (uint i = local_thread_id; i < meshlet_group_count; i += 64) {
            uint object_offset = i + store_base_id;

            uint2 cluster_group_id = uint2(work_group_id, 1);
            constant.cluster_group_id_buffer[object_offset] = cluster_group_id;
        }
    }
}
