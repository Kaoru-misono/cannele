#include "../hpp/imgui.hlsl.hpp"
#include "bindless.hlsli"
#include "color_space.hlsli"

// struct ImGuiDrawPushConstants
// {
//     float2 scale;
//     float2 translate;
//
//     uint use_font;
//     uint texture_id;
//     uint sampler_id;
// };
PUSHCONSTANTS(ImGuiDrawPushConstants, push_constants);

struct VSIn
{
    [[vk::location(0)]] float2 position : POSITION;
    [[vk::location(1)]] float2 uv  : TEXCOORD0;
    [[vk::location(2)]] float4 color : COLOR0;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 color : COLOR0;
};

void main_vs(in VSIn input, out VSOut output)
{
    output.position = float4(input.position.xy * push_constants.scale + push_constants.translate, 0.0, 1.0);
    output.position.y *= -1.0;
    output.color = input.color;
    output.uv  = input.uv;
};

void main_fs(in VSOut input, out float4 out_color : SV_Target0)
{
    Texture2D<float4> input_texture = T_BINDLESS(Texture2D, float4, push_constants.texture_id);
    SamplerState sampler = BINDLESS(SamplerState, push_constants.sampler_id);

    float4 sampler_color = input_texture.Sample(sampler, input.uv);
    if (push_constants.use_font)
    {
        // Only a8 store all data.
        sampler_color = sampler_color.a;
    }
    else
    {
        // We assume the input color can do correct color IO.
        // eg: linear rec709 store in unorm, srgb store in _srgb, so the hardware can convert color correctly.
    }

    // // ImGui color default in gamma rec709.
    float4 lerp_color = input.color;
    lerp_color.xyz = rec709GammaDecode(lerp_color.xyz);

    // Default in linearRec709, UI always draw in SRGB color buffer, so just output use hardware convert is fine.
    out_color = lerp_color * sampler_color;
}

