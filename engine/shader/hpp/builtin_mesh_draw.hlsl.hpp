#ifndef BUILTIN_MESH_DRAW_HLSL_HPP
#define BUILTIN_MESH_DRAW_HLSL_HPP

#include "common.hlsl.hpp"

NAMESPACE_CANNELE_BEGIN

    struct BuiltinMeshDrawPushConstants
    {
        float3 color;
        uint camera_view_id;

        float3 offset;
        float scale;
    };

NAMESPACE_CANNELE_END

#endif
