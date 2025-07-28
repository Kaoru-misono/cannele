#ifndef MESH_HLSL_HPP
#define MESH_HLSL_HPP

#include "common.hlsl.hpp"

#define NANITE_MESHLET_MAX_VERTEX_COUNT 255
#define NANITE_MESHLET_MAX_TRIANGLE_COUNT 128
#define NANITE_MAX_LOD_COUNT 12
#define NANITE_MAX_BVH_LEVEL_COUNT 14
#define NANITE_BVH_LEVEL_NODE_COUNT 8

#define CLUSTER_GROUP_MERGE_MAX_COUNT 4

NAMESPACE_CANNELE_BEGIN

    struct GLTFBVHNode
    {
        float4 sphere;
        uint children[NANITE_BVH_LEVEL_NODE_COUNT];

        uint bvh_node_count;
        uint leaf_meshlet_group_offset;
        uint leaf_meshlet_group_count;
    };

    struct GLTFMeshletGroup
    {
        float3 cluster_position_center;
        float parent_error;

        float3 parent_position_center;
        float error;

        uint meshlet_offset;
        uint meshlet_count;
    };

    struct GLTFMeshlet
    {
        float3 pos_min;
        uint data_offset;

        float3 pos_max;
        uint vertex_triangle_count;

        float3 cone_axis;
        float cone_cutoff;

        float3 cone_apex;
        uint lod;
    };

NAMESPACE_CANNELE_END

#endif // MESH_HLSL_HPP
