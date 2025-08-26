#pragma once

#include "common.slang.hpp"

namespace cannele
{
    struct BuiltinMeshDrawPushConstants
    {
        float3 color;
        float pad_0;
        float3 offset;
        float scale;
        descriptor::BufferHandle frame_view_buffer;
    };
}
