#pragma once

#include <math/type.hpp>

namespace cannele
{
    struct StorageDouble4
    {
        uint2 x;
        uint2 y;
        uint2 z;
        uint2 w;
    };

    struct FrameViewData
    {
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
}
