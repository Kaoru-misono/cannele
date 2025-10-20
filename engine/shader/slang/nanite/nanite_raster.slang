#include "nanite_common.slang"

import material;
import camera;
import scene;
import bindless;

[numthreads(1, 1, 1)]
void main_assemble_indirect_cmd_cs(
    uniform StructuredBufferHandle<uint> meshlet_counter,
    uniform RWStructuredBufferHandle<uint4> indirect_param_buffer
)
{
    uint meshlet_count = meshlet_counter[0];

    uint4 indirect_cmd;
    indirect_cmd.x = (meshlet_count + 64 * 4 - 1) / (64 * 4);
    indirect_cmd.y = 1;
    indirect_cmd.z = 1;
    indirect_cmd.w = 1;

    indirect_param_buffer[0] = indirect_cmd;
}

[numthreads(64, 1, 1)]
void main_filter_meshlet_cmd_cs(
    uint thread_id: SV_DispatchThreadID,
    uniform StructuredBufferHandle<FrameViewData> camera_view,
    uniform StructuredBufferHandle<GpuScene> gpu_scene,
    uniform StructuredBufferHandle<uint> meshlet_counter,
    uniform RWStructuredBufferHandle<uint> passed_meshlet_counter,
    uniform StructuredBufferHandle<uint3> meshlet_cmd_buffer,
    uniform RWStructuredBufferHandle<uint3> passed_meshlet_cmd_buffer
)
{
    FrameViewData view = camera_view[0];
    GpuScene scene = gpu_scene[0];

    uint meshlet_count = meshlet_counter[0];

    bool visible[4] = {false};
    uint3 cmd[4];
    Meshlet meshlet;

    uint cached_object_id = ~0u;
    uint cached_meshlet_id = ~0u;

    uint visible_count = 0;
    for (uint i = 0; i < 4; i++) {
        uint load_id = thread_id * 4 + i;

        if (load_id >= meshlet_count) {
            break;
        }

        cmd[i] = meshlet_cmd_buffer[load_id];

        uint instance_id = cmd[i].x;
        uint meshlet_id = cmd[i].y;

        // InstanceData instance_info = scene.instance_datas[instance_id];
        // GltfPrimitiveInfo primitive_info = scene.gltf_primitive_details[instance_info.primitive_detail_index];
        // GltfPrimitiveDataBuffers primitive_data = scene.gltf_primitive_datas[primitive_info.data_buffer_index];
        // Meshlet meshlet = primitive_data.meshlets[meshlet_id];

        visible[i] = true;

        if (visible[i]) {
            visible_count++;
        }
    }

    uint lane_offset = WavePrefixSum(visible_count);
    uint visible_count_wave = WaveActiveSum(visible_count);

    uint wave_offset = 0;
    if (WaveIsFirstLane()) {
        InterlockedAdd(passed_meshlet_counter[0], visible_count_wave, wave_offset);
    }
    wave_offset = WaveReadLaneFirst(wave_offset);

    uint local_store_offset = 0;
    for (uint i = 0; i < 4; i++) {
        if (visible[i]) {
            uint cmd_id = wave_offset + lane_offset + local_store_offset;
            passed_meshlet_cmd_buffer[cmd_id] = cmd[i];
            local_store_offset++;
        }
    }
}

struct Vertex
{
    float4 clip_position : SV_Position;
    nointerpolation uint index_in_cmd_buffer: TEXCOORD0;
};

struct PrimitiveAttributes
{
    uint primitive_id: SV_PrimitiveID;
    bool culled: SV_CullPrimitive;
};

groupshared float s_vertices_r[k_nanite_max_meshlet_vertices];
groupshared float s_vertices_g[k_nanite_max_meshlet_vertices];
groupshared float s_vertices_b[k_nanite_max_meshlet_vertices];

struct NaniteDebugData
{
    uint vertex_count;
    uint triangle_count;
    uint instance_id;
    uint meshlet_id;
    float4 position_local_space[3];
    uint3 triangle_indices;
    uint pad_0;
};

[[vk::binding(0, 0)]]
ByteAddressBuffer g_buffer[];

[shader("mesh")]
[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void main_nanite_mesh_pass_ms(
    uint dispatch_thread_id: SV_DispatchThreadID,
    uint group_thread_id: SV_GroupThreadID,
    uint group_id: SV_GroupID,
    out vertices Vertex verts[k_nanite_max_meshlet_vertices],
    out primitives PrimitiveAttributes prims[k_nanite_max_meshlet_triangles],
    out indices uint3 indices[k_nanite_max_meshlet_triangles],
    uniform StructuredBufferHandle<FrameViewData> camera_view,
    uniform StructuredBufferHandle<uint4> draw_meshlet_cmd,
    uniform StructuredBufferHandle<GpuScene> scene,
    uniform RWStructuredBufferHandle<NaniteDebugData> debug_buffer
)
{
    FrameViewData view = camera_view[0];
    uint4 draw_cmd = draw_meshlet_cmd[group_id];

    GpuScene scene = scene[0];

    uint instance_id = draw_cmd.x;
    uint meshlet_id = draw_cmd.y;


    InstanceData instance_info = scene.instance_datas[instance_id];
    GltfPrimitiveInfo primitive_info = scene.gltf_primitive_details[instance_info.primitive_detail_index];
    GltfPrimitiveDataBuffers primitive_data_info = scene.gltf_primitive_datas[primitive_info.data_buffer_index];
    Meshlet meshlet = primitive_data_info.meshlets[meshlet_id];
    // GltfMaterialData material_info = scene.gltf_primitive_materials[instance_info.material_data_index];

    // FIXME: Directly use handle to Load will compile error, maybe slang's bug.
    uint2 handle = uint2(primitive_data_info.positions);

    ByteAddressBuffer position_datas = g_buffer[handle.x];
    StructuredBufferHandle<uint> meshlet_datas = primitive_data_info.meshlet_datas;

    const uint vertices_count = unpack_vertex_count(meshlet.vertex_triangle_count);
    const uint triangles_count = unpack_triangle_count(meshlet.vertex_triangle_count);

    SetMeshOutputCounts(vertices_count, triangles_count);

    for (uint i = group_thread_id; i < vertices_count; i += 128) {
        uint vertices_sample_offset = meshlet.data_offset + i;
        uint vertices_index = meshlet_datas[vertices_sample_offset];
        uint indices_id = primitive_info.vertex_offset + vertices_index;

        float4x4 matrix_local_to_world = instance_info.matrix_local_to_world;
        float4x4 mvp = mul(view.world_to_clip_matrix, matrix_local_to_world);

        float3 position_local_space = position_datas.Load<float3>(indices_id * sizeof(float3));

        Vertex out;
        float4 position_clip_space = mul(mvp, float4(position_local_space, 1.0));
        out.clip_position = position_clip_space;

        s_vertices_r[i] = position_clip_space.x;
        s_vertices_g[i] = position_clip_space.y;
        s_vertices_b[i] = position_clip_space.z;

        if (vertices_count == 3) {
            out.index_in_cmd_buffer = draw_cmd.z;
        } else {
            out.index_in_cmd_buffer = -1;
        }
        // out.index_in_cmd_buffer = draw_cmd.z;

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

        float3 position_clip_space_0 = float3(s_vertices_r[triangle_indices.x], s_vertices_g[triangle_indices.x], s_vertices_b[triangle_indices.x]);
        float3 position_clip_space_1 = float3(s_vertices_r[triangle_indices.y], s_vertices_g[triangle_indices.y], s_vertices_b[triangle_indices.y]);
        float3 position_clip_space_2 = float3(s_vertices_r[triangle_indices.z], s_vertices_g[triangle_indices.z], s_vertices_b[triangle_indices.z]);

        bool culled = false;
        // Back face culling.
        // https://www.cs.cmu.edu/afs/cs/academic/class/15869-f11/www/readings/olano97_homogeneous.pdf
        culled = determinant(float3x3(position_clip_space_0, position_clip_space_1, position_clip_space_2)) <= 0;

        // Near plane culling.
        if (!culled) {
            culled = (position_clip_space_0.z <= 0 && position_clip_space_1.z <= 0 && position_clip_space_2.z <= 0);
        }

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
    out_id = encode_triangle_and_array_index(primitive_id, input.index_in_cmd_buffer);
}
