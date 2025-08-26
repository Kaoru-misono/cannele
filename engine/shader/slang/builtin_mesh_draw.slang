#include "camera.slang"
#include "bindless.slang"

struct BuiltinMeshDrawPushConstants
{
    float3 color;
    uint camera_view_id;

    float3 offset;
    float scale;
};
PUSHCONSTANTS(BuiltinMeshDrawPushConstants, push_constants);

struct VSIn
{
    [[vk::location(0)]] float3 position : POSITION0;
    [[vk::location(1)]] float3 normal   : NORMAL0;
    [[vk::location(2)]] float2 uv       : TEXCOORD0;
};

struct VSOut
{
    float4 position : SV_Position;

    [[vk::location(0)]] float3 normal : NORMAL0;
    [[vk::location(1)]] float2 uv     : TEXCOORD0;
};

[shader("vertex")]
VSOut main_built_in_mesh_vs(VSIn input)
{
    VSOut output;

    PerFrameCameraView view = StructuredBuffer<PerFrameCameraView>.Handle(uint2(push_constants.camera_view_id, 0))[0];

    float3 position = input.position * push_constants.scale + push_constants.offset;
    output.position = mul(view.world_to_clip_matrix, float4(position, 1.0));

    output.normal = input.normal;
    output.uv = input.uv;

    return output;
}

[shader("fragment")]
void main_built_in_mesh_fs(in VSOut vs_output, out float4 out_color: SV_Target0)
{
    out_color = float4(push_constants.color, 1.0);
}
