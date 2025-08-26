#pragma once

#include "common.slang.hpp"

namespace cannele
{
    struct InstanceData
    {
        float4x4 matrix_local_to_world;
        float4x4 matrix_world_to_local;

        float4x4 matrix_local_to_world_last_frame;

        uint primitive_detail_index;
        uint material_data_index;
    };

    struct alignas(16) GltfPrimitiveInfo
    {
        float3 position_min;
        uint vertex_offset;

        float3 position_max;
        uint vertex_count;

        float3 position_average;
        uint pad;

        uint meshlet_offset;
        uint color_0_offset;
        uint smooth_normal_offset;
        uint texcoord_1_offset;

        uint bvh_node_offset;
        uint meshlet_group_offset;
        uint meshlet_group_indices_offset;
        uint meshlet_group_count;

        uint lod_0_indices_offset;
        uint lod_0_indices_count;

        uint data_buffer_index;
    };

    struct Meshlet
    {
        float3 position_min;
        uint data_offset;

        float3 position_max;
        uint vertex_triangle_count;

        float3 cone_axis;
        float cut_off;

        float3 cone_apex;
        uint lod;
    };

    struct alignas(16) GltfPrimitiveDataBuffers
    {
        descriptor::BufferHandle meshlets;
        descriptor::BufferHandle positions;
        descriptor::BufferHandle normals;
        descriptor::BufferHandle texcoords_0;

        descriptor::BufferHandle tangents;
        descriptor::BufferHandle texcoords_1;
        descriptor::BufferHandle colors;
        descriptor::BufferHandle smooth_normals;

        descriptor::BufferHandle meshlet_datas;
        descriptor::BufferHandle bvh_nodes;
        descriptor::BufferHandle meshlet_groups;
        descriptor::BufferHandle meshlet_group_indices;

        descriptor::BufferHandle lod_0_indices;
    };

    struct GltfMaterialData
    {
        uint alpha_mode;
        float alpha_cutoff;
        uint is_two_side;

        float4 base_color_factor;

        float3 emissive_factor;

        float metallic_factor;
        float roughness_factor;

        descriptor::Sampler2DHandle base_color;
        descriptor::Sampler2DHandle emissive;
        descriptor::Sampler2DHandle normal;
        descriptor::Sampler2DHandle metallic_roughness;

        float normal_factor_scale;
        float occlusion_texture_strength;
        uint material_type;
    };

    struct GpuScene
    {
        descriptor::BufferHandle gltf_objects;
        descriptor::BufferHandle gltf_primitive_details;
        descriptor::BufferHandle gltf_primitive_datas;
        descriptor::BufferHandle gltf_primitive_materials;

        uint gltf_object_count;
        uint pad;
    };
}
