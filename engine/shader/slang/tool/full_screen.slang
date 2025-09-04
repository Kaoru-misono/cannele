struct FullScreenVSOutput
{
    float4 position: SV_Position;
    float2 uv: TEXCOORD0;
};

[shader("vertex")]
FullScreenVSOutput main_fullscreen_vs(uint vertex_id: SV_VertexID)
{
    FullScreenVSOutput output;

    output.uv = float2((vertex_id & 2), (vertex_id & 1) << 1);

    output.position = float4(float2(output.uv * float2(2, -2) + float2(-1, 1)), 0.0, 1.0);

    return output;
}
