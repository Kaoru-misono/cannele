#include "nanite_common.slang"

import camera;
import scene;
import bindless;

groupshared uint shared_store_base_id;
groupshared uint shared_meshlet_group_count;
groupshared bool shared_is_visible;

[shader("compute")]
[numthreads(64, 1, 1)]
void main_instance_culling_cs(
    uint workgroup_id: SV_GroupID,
    uint local_thread_id: SV_GroupIndex,
    uniform StructuredBufferHandle<FrameViewData> frame_views,
    uniform RWStructuredBufferHandle<uint> cluster_group_counts,
    uniform RWStructuredBufferHandle<uint2> cluster_group_ids,
    uniform StructuredBufferHandle<GpuScene> scene
)
{
    FrameViewData view = frame_views[0];
    GpuScene scene = scene[0];

    bool is_visible = true;
    bool is_first_thread = (local_thread_id == 0);

    uint bvh_node_count = 0;
    uint meshlet_group_count = 0;
    uint store_base_id = 0;
    if (is_first_thread) {
        InstanceData instance_info = scene.instance_datas[workgroup_id];
        GltfPrimitiveInfo primitive_info = scene.gltf_primitive_details[instance_info.primitive_detail_index];

        float4x4 matrix_local_to_world = instance_info.matrix_local_to_world;
        float4x4 matrix_local_to_clip = mul(view.world_to_clip_matrix, matrix_local_to_world);

        float3 position_min = primitive_info.position_min;
        float3 position_max = primitive_info.position_max;
        float3 position_center = (position_min + position_max) * 0.5;
        float3 extent = position_max - position_center;

        // TODO: Frustum visible culling.

        if (is_visible) {
            GltfPrimitiveDataBuffers primitive_data_info = scene.gltf_primitive_datas[primitive_info.data_buffer_index];
            // TODO: BVH node.
            meshlet_group_count += primitive_info.meshlet_group_count;
            InterlockedAdd(cluster_group_counts[0], meshlet_group_count, store_base_id);
        }
    }

    if (is_first_thread) {
        shared_store_base_id = store_base_id;
        shared_meshlet_group_count = meshlet_group_count;
        shared_is_visible = is_visible;
    }
    GroupMemoryBarrierWithGroupSync();

    meshlet_group_count = shared_meshlet_group_count;
    store_base_id = shared_store_base_id;
    is_visible = shared_is_visible;

    if (is_visible) {
        for (uint i = local_thread_id; i < meshlet_group_count; i += 64) {
            uint write_index = store_base_id + i;

            uint2 cluster_group_id = uint2(workgroup_id, i); // instance id, local offset
            cluster_group_ids[write_index] = cluster_group_id;
        }
    }
}


[shader("compute")]
[numthreads(64, 1, 1)]
void main_cluster_group_culling_cs(
    uint thread_id: SV_DispatchThreadID,
    uniform StructuredBufferHandle<FrameViewData> frame_views,
    uniform StructuredBufferHandle<uint> cluster_group_counts,
    uniform StructuredBufferHandle<uint2> cluster_group_ids,
    uniform StructuredBufferHandle<GpuScene> scene,
    uniform RWStructuredBufferHandle<uint> meshlet_counter,
    uniform RWStructuredBufferHandle<uint3> meshlet_cmds,
)
{
    uint meshlet_group_count = cluster_group_counts[0];
    if (thread_id >= meshlet_group_count) {
        return;
    }

    FrameViewData view = frame_views[0];
    GpuScene scene = scene[0];

    uint2 cluster_id = cluster_group_ids[thread_id];
    uint instance_id = cluster_id.x;
    uint cluster_group_id = cluster_id.y;

    InstanceData instance_info = scene.instance_datas[instance_id];
    GltfPrimitiveInfo primitive_info = scene.gltf_primitive_details[instance_info.primitive_detail_index];
    GltfPrimitiveDataBuffers primitive_data_info = scene.gltf_primitive_datas[primitive_info.data_buffer_index];

    StructuredBufferHandle<Meshlet> meshlet_buffer = primitive_data_info.meshlets;
    StructuredBufferHandle<MeshletGroup> meshlet_group_buffer = primitive_data_info.meshlet_groups;
    StructuredBufferHandle<uint> meshlet_group_indices = primitive_data_info.meshlet_group_indices;

    uint meshlet_group_index = primitive_info.meshlet_group_offset + cluster_group_id;
    MeshletGroup meshlet_group = meshlet_group_buffer[meshlet_group_index];

    float4x4 matrix_local_to_world = instance_info.matrix_local_to_world;

    uint visible_meshlet_count = 0;
    uint visible_meshlet_ids[k_cluster_group_merge_max_count];

    for (uint i = 0; i < meshlet_group.meshlet_count; i++) {
        uint index = meshlet_group.meshlet_offset + primitive_info.meshlet_group_indices_offset + i;
        uint meshlet_index = primitive_info.meshlet_offset + meshlet_group_indices[index];
        Meshlet meshlet = meshlet_buffer[meshlet_index];
        if (meshlet.lod == 0) {
            visible_meshlet_ids[visible_meshlet_count] = meshlet_index;
            visible_meshlet_count++;
        }
    }

    uint visible_meshlet_count_wave_sum = WaveActiveSum(visible_meshlet_count);
    uint store_base_id = 0;
    if (WaveIsFirstLane()) {
        InterlockedAdd(meshlet_counter[0], visible_meshlet_count_wave_sum, store_base_id);
    }
    store_base_id = WaveReadLaneFirst(store_base_id);

    uint relative_offset = WavePrefixSum(visible_meshlet_count);
    for (uint i = 0; i < visible_meshlet_count; i++) {
        uint store_index = store_base_id + relative_offset + i;
        uint3 cmd_paramenter = uint3(instance_id, visible_meshlet_ids[i], store_index);
        meshlet_cmds[store_index] = cmd_paramenter;
    }
}
