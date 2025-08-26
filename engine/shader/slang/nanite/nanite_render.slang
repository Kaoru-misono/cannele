#include "nanite_common.slang"

import material;
import camera;
import scene;
import bindless;

struct InstancePushConstant
{
    StructuredBufferHandle<FrameViewData> camera_view;
    StructuredBufferHandle<uint3> draw_meshlet_cmd;
    StructuredBufferHandle<GpuScene> scene;
};
[[vk::push_constant]] InstancePushConstant constant;

struct Vertex
{
    float4 clip_position: SV_Position;
    nointerpolation uint cluster_id: TEXCOORD0;
};

struct PrimitiveAttributes
{
    uint primitive_id: SV_PrimitiveID;
    bool culled: SV_CullPrimitive;
};

groupshared float shared_vertices_r[NANITE_MAX_MESHLET_VERTICES];
groupshared float shared_vertices_g[NANITE_MAX_MESHLET_VERTICES];
groupshared float shared_vertices_b[NANITE_MAX_MESHLET_VERTICES];

[shader("mesh")]
[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void main_nanite_mesh_pass_ms(
    uint dispatch_thread_id: SV_DispatchThreadID,
    uint group_thread_id: SV_GroupThreadID,
    uint group_id: SV_GroupID,
    out vertices Vertex verts[NANITE_MAX_MESHLET_VERTICES],
    out primitives PrimitiveAttributes prims[NANITE_MAX_MESHLET_TRIANGLES],
    out indices uint3 indices[NANITE_MAX_MESHLET_TRIANGLES]
)
{
    FrameViewData view = constant.camera_view[0];
    uint3 draw_cmd = constant.draw_meshlet_cmd[group_id];

    GpuScene scene = constant.scene[0];

    uint object_id = draw_cmd.x;
    uint meshlet_id = draw_cmd.y;

    InstanceData object_info = scene.gltf_objects[object_id];
    GltfPrimitiveInfo primitive_info = scene.gltf_primitive_details[object_info.primitive_detail_index];
    GltfPrimitiveDataBuffers primitive_data_info = scene.gltf_primitive_datas[primitive_info.data_buffer_index];
    Meshlet meshlet = primitive_data_info.meshlets[meshlet_id];
    GltfMaterialData material_info = scene.gltf_primitive_materials[object_info.material_data_index];

    StructuredBufferHandle<float3> position_datas = primitive_data_info.positions;
    StructuredBufferHandle<uint> meshlet_datas = primitive_data_info.meshlet_datas;

    // TODO: Unpack.
    const uint vertices_count = meshlet.vertex_triangle_count;
    const uint triangles_count = meshlet.vertex_triangle_count;

    SetMeshOutputCounts(vertices_count, triangles_count);

    for (uint i = group_thread_id; i < vertices_count; i += 128) {
        uint vertices_sample_offset = meshlet.data_offset + i;
        uint vertices_index = meshlet_datas[vertices_sample_offset];
        uint indices_id = primitive_info.vertex_offset + vertices_index;

        float4x4 matrix_local_to_world = object_info.matrix_local_to_world;
        float4x4 mvp = mul(view.world_to_clip_matrix, matrix_local_to_world);

        float3 position_local_space = position_datas[indices_id];

        Vertex out;
        const float4 position_clip_space = mul(mvp, float4(position_local_space, 1.0));
        out.clip_position = position_clip_space;

        shared_vertices_r[i] = position_clip_space.x;
        shared_vertices_g[i] = position_clip_space.y;
        shared_vertices_b[i] = position_clip_space.z;

        out.cluster_id = draw_cmd.z;

        verts[i] = out;
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint i = group_thread_id; i < triangles_count; i += 128) {
        uint triangle_indices_sample_offset = meshlet.data_offset + vertices_count + i;
        uint index_triangle = meshlet_datas[triangle_indices_sample_offset];

        uint3 triangle_indices;
        triangle_indices.x = (index_triangle >> (0 * 8)) & 0xff;
        triangle_indices.y = (index_triangle >> (1 * 8)) & 0xff;
        triangle_indices.z = (index_triangle >> (2 * 8)) & 0xff;

        indices[i] = triangle_indices;

        float3 position_clip_space_0 = float3(shared_vertices_r[triangle_indices.x], shared_vertices_g[triangle_indices.x], shared_vertices_b[triangle_indices.x]);
        float3 position_clip_space_1 = float3(shared_vertices_r[triangle_indices.y], shared_vertices_g[triangle_indices.y], shared_vertices_b[triangle_indices.y]);
        float3 position_clip_space_2 = float3(shared_vertices_r[triangle_indices.z], shared_vertices_g[triangle_indices.z], shared_vertices_b[triangle_indices.z]);

        bool culled = false;

        culled = (position_clip_space_0.z <= 0 && position_clip_space_1.z <= 0 && position_clip_space_2.z <= 0);

        prims[i].primitive_id = i;
        prims[i].culled = culled;
    }
}

[shader("fragment")]
void main_nanite_visibility_buffer_pass_fs(
    in uint primitive_id: SV_PrimitiveID,
    in Vertex input,
    out uint out_id: SV_Target
)
{
    out_id = encode_triangle_and_instance_id(primitive_id, input.cluster_id);
}
