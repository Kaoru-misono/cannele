#include "../hpp/structure.hpp"
#include "bindless.hlsli"
#include "color_space.hlsli"

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
    output.position = float4(input.position.xy * pushConstants.scale + pushConstants.translate, 0.0, 1.0);
    output.position.y *= -1.0;
    output.color = input.color;
    output.uv  = input.uv;
};

void main_fs(in VSOut input, out float4 outColor : SV_Target0)
{
    Texture2D<float4> inputTexture = T_BINDLESS(Texture2D, float4, pushConstants.textureId);
    SamplerState sampler = BINDLESS(SamplerState, pushConstants.samplerId);

    float4 sampledColor = inputTexture.Sample(sampler, input.uv);
    if (pushConstants.useFont)
    {
        // Only a8 store all data.
        sampledColor = sampledColor.a;
    }
    else
    {
        // We assume the input color can do correct color IO.
        // eg: linear rec709 store in unorm, srgb store in _srgb, so the hardware can convert color correctly.
    }

    // // ImGui color default in gamma rec709.
    float4 lerpColor = input.color;
    lerpColor.xyz = rec709GammaDecode(lerpColor.xyz);

    // Default in linearRec709, UI always draw in SRGB color buffer, so just output use hardware convert is fine.
    outColor = lerpColor * sampledColor;
}

