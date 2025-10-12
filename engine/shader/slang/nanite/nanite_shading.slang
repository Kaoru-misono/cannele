#include "nanite_common.slang"

import bindless;
import camera;
import scene;
import math;

groupshared uint s_tile_marker_r[8][8];
groupshared uint s_tile_marker_g[8][8];
groupshared uint s_tile_marker_b[8][8];
groupshared uint s_tile_marker_a[8][8];
groupshared uint4 s_out[16];

[shader("compute")]
[numthreads(64, 1, 1)]
void main_tile_marker_cs(
    uint2 work_group_id: SV_GroupID,
    uint local_thread_id: SV_GroupIndex,
    uniform StructuredBufferHandle<FrameViewData> view_buffer,
    uniform StructuredBufferHandle<GpuScene> scene_buffer,
    uniform Texture2DHandle<uint> visibility_texture,
    uniform float2 visibility_texel_size,
    uniform RWTexture2DHandle<uint4> tile_marker_texture,
    uniform SamplerStateHandle gather_sampler,
    uniform StructuredBufferHandle<uint3> meshlet_cmd_buffer,
)
{
    FrameViewData view = view_buffer[0];
    GpuScene scene = scene_buffer[0];

    uint2 remap_id = remap8x8(local_thread_id);

    uint4 shading_tile_marker = 0;

    [unroll(2)]
    for (uint y = 0; y < 2; y++) {
        [unroll(2)]
        for (uint x = 0; x < 2; x++) {
            // The visibility texture is divided into tiles. Each workgroup handles a 32x32(8x4 x 8x4) pixel tile.
            // Within a workgroup, we have 8x8 threads (remap_id), each thread is responsible for a 4x4 pixel block.
            // Each thread processes 2x2 pixels at a time in the inner loops (x,y), so the pixel offset of the thread is 4 * remap_id.
            // The final gather position for a pixel is: work_group_id*32 + 4*remap_id + 2*uint2(x,y).
            uint2 gather_pos = work_group_id * 32 + 4 * remap_id + 2 * uint2(x, y);
            float2 gather_uv = (gather_pos + 1.0) * visibility_texel_size;
            uint4 gather_pack_id_x4 = visibility_texture.Gather(gather_sampler, gather_uv);

            [unroll(4)]
            for (uint i = 0; i < 4; i++) {
                uint shading_type = 0;
                if (gather_pack_id_x4[i] != 0) {
                    uint triangle_id = 0;
                    uint index_in_cmd_buffer = 0;
                    decode_triangle_and_array_index(gather_pack_id_x4[i], triangle_id, index_in_cmd_buffer);

                    uint3 cmd = meshlet_cmd_buffer[index_in_cmd_buffer];
                    shading_type = cmd.x;
                    uint instance_id = cmd.x;

                    InstanceData instance_info = scene.instance_datas[instance_id];
                    // GltfMaterialData material_info = scene.gltf_primitive_materials[instance_info.material_data_index];
                    // TODO: Set shading_type based on material properties.
                    shading_type = 1;
                }
                shading_tile_marker[shading_type / 32] |= 1u << (shading_type % 32);
            }
        }
    }

    // `groupshared` aggregation buffer: one entry per 2x2 output cell inside this workgroup
    // `remapId` ranges 0..7; output indices are `remapId`/2 => 0..3 in each dim -> 4x4 = 16 slots
    // Iitialize (only a few threads need to initialize; do it once)
    if (local_thread_id < 16) {
        s_out[local_thread_id] = {};
    }
    GroupMemoryBarrierWithGroupSync();

    // Compute which output slot this thread contributes to (2x2 block -> output 4x4)
    uint out_x = remap_id.x / 2;
    uint out_y = remap_id.y / 2;
    uint out_index = out_y * 4 + out_x;

    // Atomically OR our mask into the group's output slot (per-component)
    InterlockedOr(s_out[out_index].x, shading_tile_marker.x);
    InterlockedOr(s_out[out_index].y, shading_tile_marker.y);
    InterlockedOr(s_out[out_index].z, shading_tile_marker.z);
    InterlockedOr(s_out[out_index].w, shading_tile_marker.w);
    GroupMemoryBarrierWithGroupSync();

    // Only one thread per output slot writes back to global RWTexture
    // choose e.g. the thread with even remapId x/y and that maps to this out_index
    // we can simply let threads with remapId.x % 2 == 0 && remapId.y % 2 == 0 write:
    if (remap_id.x % 2 == 0 && remap_id.y % 2 == 0) {
         uint2 store_index = work_group_id * 4 + uint2(out_x, out_y);
         uint4 tile_result = s_out[out_index];

        tile_marker_texture[store_index] = tile_result;
    }
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main_tile_prepare_cs(
    uint2 work_group_id: SV_GroupID,
    uint local_thread_id: SV_GroupIndex,
    uniform Texture2DHandle<uint4> tile_marker_texture,
    uniform uint marker_index,
    uniform uint marker_bit,
    uniform uint2 marker_dim,
    uniform RWStructuredBufferHandle<uint> tile_count_buffer,
    uniform RWStructuredBufferHandle<uint2> tile_cmd_buffer,
)
{
    uint2 base_index = work_group_id * 16 + remap8x8(local_thread_id);

    static const uint invalid_marker = ~0u;

    uint valid_tile_count = 0;

    uint4 marker_array[4] = {};
    [unroll(2)]
    for (uint y = 0; y < 2; y++) {
        [unroll(2)]
        for (uint x = 0; x < 2; x++) {
            uint2 sample_index = base_index + uint2(x, y) * 8;
            if (all(sample_index < marker_dim)) {
                uint4 marker = tile_marker_texture[sample_index];
                if ((marker[marker_index] & marker_bit) != 0u) {
                    valid_tile_count++;
                }
            }
        }
    }

    uint tile_count_wave_sum = WaveActiveSum(valid_tile_count);
    uint store_base_index = 0;
    if (WaveIsFirstLane()) {
        InterlockedAdd(tile_count_buffer[0], tile_count_wave_sum, store_base_index);
    }
    store_base_index = WaveReadLaneFirst(store_base_index);

    uint prefix_inclusive = WavePrefixSum(valid_tile_count);
    uint prefix_before = prefix_inclusive - valid_tile_count;

    uint relative_offset = store_base_index + prefix_before;
    uint local_idx = 0;
    [unroll(2)]
    for (uint y = 0; y < 2; ++y) {
        [unroll(2)]
        for (uint x = 0; x < 2; ++x) {
            uint2 sample_index = base_index + uint2(x, y) * 8;
            if (all(sample_index < marker_dim)) {
                uint4 marker = tile_marker_texture[sample_index];
                if ((marker[marker_index] & marker_bit) != 0u) {
                    // Compute global slot for this found tile.
                    uint write_index = relative_offset + local_idx;
                    // Store sample_index * 8 to match original semantics.
                    tile_cmd_buffer[write_index] = sample_index * 8;
                    local_idx++;
                }
            }
        }
    }
}

[numthreads(1, 1, 1)]
[shader("compute")]
void main_tile_assemble_indirect_cmd_cs(
    uniform StructuredBufferHandle<uint> tile_count_buffer,
    uniform RWStructuredBufferHandle<uint4> indirect_buffer,
)
{
    uint tile_count = tile_count_buffer[0];

    uint4 cmd_param = {1};
    cmd_param.x = (tile_count + 3) / 4;

    indirect_buffer[0] = cmd_param;
}
