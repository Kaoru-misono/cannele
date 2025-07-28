#ifndef IMGUI_HLSL_HPP
#define IMGUI_HLSL_HPP

#include "common.hlsl.hpp"

NAMESPACE_CANNELE_BEGIN

    struct ImGuiDrawPushConstants
    {
        float2 scale;
        float2 translate;

        uint use_font;
        uint texture_id;
        uint sampler_id;
    };

NAMESPACE_CANNELE_END

#endif // IMGUI_HLSL_HPP
