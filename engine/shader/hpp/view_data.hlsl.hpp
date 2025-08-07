#ifndef VIEW_DATA_HLSL_HPP
#define VIEW_DATA_HLSL_HPP

#include "common.hlsl.hpp"

NAMESPACE_CANNELE_BEGIN

    struct GpuBasicData
    {
        uint frame_count;
        uint frame_count_mod_8;
        uint GLTF_primitive_detail_buffer;
        uint GLTF_primitive_data_buffer;

        uint GLTF_material_buffer;
        uint GLTF_object_count;
        uint GLTF_object_buffer;
        uint GLTF_object_vertices_buffer;

        uint debug_line_count;
        uint debug_line_max_count;
        uint point_clamp_edge_sampler;
        uint linear_clamp_edge_sampler;

        float2 halton23_16tap[16];

        uint brdf_lut;
        uint pad0;
        uint pad1;
        uint pad2;
    };

    struct PerFrameCameraView
    {
        GpuBasicData basic_data;

        float4x4 world_to_view_matrix;
        float4x4 view_to_world_matrix;

        float4x4 view_to_clip_matrix;
        float4x4 clip_to_view_matrix;

        float4x4 world_to_clip_matrix;
        float4x4 clip_to_world_matrix;
        float4x4 world_to_clip_matrix_pre_frame;
        float4x4 clip_to_world_matrix_pre_frame;

        float4 frustum_plane[6];
        float4 frustum_plane_pre_frame[6];

        float4 viewport;

        StorageDouble4 vector_world_origin_to_camera;
        StorageDouble4 camera_position_world_space;

        StorageDouble4 camera_position;
        StorageDouble4 camera_position_pre_frame;

        float camera_fovy;
        float z_near;
        float z_far;
        uint camera_cut;

        float4 jitter_data;

        float3 forward;
        float pad0;
    };
    CHECK_STRUCT_ALIGNMENT(PerFrameCameraView);

NAMESPACE_CANNELE_END

#endif
